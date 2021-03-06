#include "jobsexecuter.h"
#include "sysinfo.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(LOG_JE, "jobexecuter")

JobsExecuter::JobsExecuter(const QUrl &address, QString q_cmd, QString q_rpl,
                           int aliveInterval, int aliveTTL, QString execRegex,
                           QObject *parent) :
  QObject(parent), m_url(address), m_aliveTTL(aliveTTL)
{
  execRe.setPattern(execRegex);

  m_client = new QAMQP::Client(this);
  m_client->open(m_url);
  m_client->setAutoReconnect(true);

  m_qIn = m_client->createQueue();
  m_qIn->declare(q_cmd, QAMQP::Queue::Durable);
  connect(m_qIn, &QAMQP::Queue::declared, this, [this] () {
      m_qIn->setQOS(0,1);
      m_qIn->consume();
    });
  connect(m_qIn, &QAMQP::Queue::messageReceived, this, &JobsExecuter::newCommand);

  m_exchange =  m_client->createExchange();

  m_qOut = m_client->createQueue(m_exchange->channelNumber());
  m_qOut->declare(q_rpl, QAMQP::Queue::Durable);

  connect(&aliveTimer, SIGNAL(timeout()), this, SLOT(sendAlive()));
  aliveTimer.setInterval(aliveInterval);
  aliveTimer.start();
}

void JobsExecuter::newCommand()
{
  QAMQP::MessagePtr message = m_qIn->getMessage();
  qCDebug(LOG_JE) << "JobsExecuter::newMessage " << message->payload;

  QByteArray bMessage = message->payload;
  m_qIn->ack(message);

  QJsonParseError parseResult;
  auto doc = QJsonDocument::fromJson(bMessage, &parseResult);

  if (parseResult.error != QJsonParseError::NoError) {
      qCWarning(LOG_JE) << "Error parsing message: " + parseResult.errorString();
      return;
    }

  // we need { } style messages, ignore all other :)
  if (!doc.isObject()) {
      qCWarning(LOG_JE) << "Message is not an object: " + bMessage;
      return;
    }

  QVariantMap msg = doc.object().toVariantMap();

  QString type = msg.value("type").toString();
  QString jobid = msg.value("jobId").toString();

  if (type == "exec") {
      QString cmd = msg.value("cmd").toString();

      if (!execRe.match(cmd).hasMatch()) {
        emit cmdError(jobid, "Not permited command");
        return;
      }

      QStringList args = msg.value("args").toStringList();
      QByteArray data = QByteArray::fromBase64(msg.value("stdin").toByteArray());
      QByteArray tempfile;
      QVariantList env;
      if (msg.contains("env"))
          env = msg.value("env").toList();
      if (msg.contains("tempfile"))
          tempfile = QByteArray::fromBase64(msg.value("tempfile").toByteArray());
      emit cmdExec(jobid, cmd, args, data, env, tempfile);
    } else if (type == "terminate") {
      emit cmdTerminate(jobid);
    } else if (type == "kill") {
      emit cmdKill(jobid);
    } else if (type == "close") {
      emit cmdClose(jobid);
    } else if (type == "stdin") {
      QByteArray data = QByteArray::fromBase64(msg.value("stdin").toByteArray());
      emit cmdStdin(jobid, data);
    }
}

void JobsExecuter::cmdExec(QString job, QString cmdline, QStringList args, QByteArray indata, QVariantList env, QByteArray tempfile)
{
  if (m_pool.contains(job)) {
      qCWarning(LOG_JE) << "Duplicate jobid " + job;
      return;
    }
  QProcess *process = new QProcess(this);
  m_pool.insert(job, process);
  process->setProperty("job", job);

  connect(process, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
  connect(process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(processFinised(int,QProcess::ExitStatus)));
  connect(process, SIGNAL(started()), this, SLOT(processStarted()));
  connect(process, SIGNAL(readyReadStandardError()), this, SLOT(processStderr()));
  connect(process, SIGNAL(readyReadStandardOutput()), this, SLOT(processStdout()));

  QStringList processEnv;
  if (env.empty())
  {
    processEnv << "PATH=/bin:/usr/bin";
    processEnv << "SHELL=/bin/bash";
    processEnv << "LANG=C";
    processEnv << "LC_ALL=C";
  } else {
    for (QVariant s : env)
      processEnv.append(s.toString());
  }

  process->setEnvironment(processEnv);

  if (tempfile.size() > 0) {
      QString tempfilename = tempdir.path() + '/' + job.toUtf8().toHex();
      qCDebug(LOG_JE) << "creating temp file" << tempfilename;
      QFile tfile(tempfilename);
      if (tfile.open(QIODevice::WriteOnly)) {
          tfile.write(tempfile);
          tfile.close();
          process->setProperty("tempfile", tempfilename);
          args.replaceInStrings("%tempfile%", tempfilename);
      } else {
          qCCritical(LOG_JE) << "error while creating temp file" << tempfilename;
      }
  }

  process->start(cmdline, args);
  if (!indata.isNull() && !indata.isEmpty()) {
      process->setProperty("stdin", indata);
    }
}

void JobsExecuter::cmdTerminate(QString job)
{
  if (!m_pool.contains(job)) {
      qCWarning(LOG_JE) << "Terminate of unknown job, jobid = " + job;
      return;
    }

  m_pool.value(job)->terminate();
}

void JobsExecuter::cmdKill(QString job)
{
  if (!m_pool.contains(job)) {
      qCWarning(LOG_JE) << "Kill of unknown job, jobid = " + job;
      return;
    }

  m_pool.value(job)->kill();
}

void JobsExecuter::cmdStdin(QString job, QByteArray indata)
{
    if (!m_pool.contains(job)) {
        qCWarning(LOG_JE) << "Close of unknown job, jobid = " + job;
        return;
      }

    QProcess *process = m_pool[job];
    process->write(indata);
}

void JobsExecuter::cmdClose(QString job)
{
    if (!m_pool.contains(job)) {
        qCWarning(LOG_JE) << "Close of unknown job, jobid = " + job;
        return;
      }

    QProcess *process = m_pool[job];
    process->closeWriteChannel();
}

void JobsExecuter::cmdError(QString job, QString error)
{
  QVariantMap msg;
  msg["type"] = "status";
  msg["jobId"] = job;
  msg["timestamp"] = QDateTime::currentMSecsSinceEpoch();
  msg["status"] = "error";
  msg["message"] = error;
  sendReply(msg);
}

void JobsExecuter::processError(QProcess::ProcessError error)
{
  QVariantMap msg = prepareReply("status");
  msg["status"] = "error";
  switch (error) {
    case QProcess::FailedToStart:
      msg["message"] = "Failed to start";
      break;
    case QProcess::Crashed:
      msg["message"] = "Crashed";
      break;
    case QProcess::Timedout:
      msg["message"] = "Timeout";
      break;
    case QProcess::WriteError:
      msg["message"] = "Write error";
      break;
    case QProcess::ReadError:
      msg["message"] = "Read error";
      break;
    case QProcess::UnknownError:
      msg["message"] = "Unknown error";
      break;
    default:
      msg["message"] = "unknown error id";
    }

  sendReply(msg);

}

void JobsExecuter::processFinised(int exitCode, QProcess::ExitStatus exitStatus)
{
  Q_UNUSED(exitStatus)
  QVariantMap msg = prepareReply("status");
  msg["status"] = "stoped";
  msg["exitCode"] = exitCode;
  sendReply(msg);
  if (sender()) {
    QProcess *process = qobject_cast<QProcess *>(sender());
    if (process) {
      m_pool.remove(process->property("job").toString());
      if (process->property("tempfile").isValid()) {
          QFile::remove(process->property("tempfile").toString());
      }
      process->deleteLater();
    }
  }
}

void JobsExecuter::processStarted()
{
  QVariantMap msg = prepareReply("status");
  msg["status"] = "started";
  sendReply(msg);

  if (sender()->property("stdin").isValid()) {
      QProcess *process = qobject_cast<QProcess *>(sender());
      process->write(process->property("stdin").toByteArray());
      process->setProperty("stdin", QVariant());
      process->closeWriteChannel();
    }
}

void JobsExecuter::processStdout()
{
  QVariantMap msg = prepareReply("stdOut");
  msg["data"] = qobject_cast<QProcess *>(sender())->readAllStandardOutput().toBase64();
  sendReply(msg);
}

void JobsExecuter::processStderr()
{
  QVariantMap msg = prepareReply("stdErr");
  msg["data"] = qobject_cast<QProcess *>(sender())->readAllStandardError().toBase64();
  sendReply(msg);
}

void JobsExecuter::sendAlive()
{
    QVariantMap msg;
    msg["type"] = "alive";
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    auto jobsS = m_pool.keys();
    QList<QVariant> jobs;
    for (QString job : jobsS) {
        jobs.append(job);
    }
    msg["active"] = jobs;

    // add some system information
    msg["loadavg"] = loadavg1();
    msg["ncpu"] = numCpu();
    struct memory_info mem = memInfo();
    msg["totalmem"] = mem.totalKB;
    msg["freemem"] = mem.freeKB;

    QAMQP::Exchange::MessageProperties properties;
    properties[QAMQP::Frame::Content::cpExpiration] = m_aliveTTL; // for this time of messages only 1.5 seconds of live
    m_exchange->publish(QJsonDocument::fromVariant(msg).toJson(), m_qOut->name(), properties);
    qCDebug(LOG_JE) << "alive" << msg;
}

void JobsExecuter::sendReply(QVariantMap reply)
{
  QAMQP::Exchange::MessageProperties properties;
  properties[QAMQP::Frame::Content::cpDeliveryMode] = 2; // Make message persistent
  m_exchange->publish(QJsonDocument::fromVariant(reply).toJson(), m_qOut->name(), properties);
  qCDebug(LOG_JE) << "reply" << reply;
}

QVariantMap JobsExecuter::prepareReply(QString type, QString job)
{
  QVariantMap msg;
  msg["type"] = type;
  if (job.isNull() || job.isEmpty()) {
      if (sender()) {
          msg["jobId"] = sender()->property("job").toString();
        }
    } else {
      msg["jobId"] = job;
    }
  msg["timestamp"] = QDateTime::currentMSecsSinceEpoch();

  return msg;
}

