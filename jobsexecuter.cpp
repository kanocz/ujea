#include "jobsexecuter.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

#include <QDebug>

JobsExecuter::JobsExecuter(const QUrl &address, QString hostname, QObject *parent) :
  QObject(parent), m_url(address), m_hostname(hostname)
{
  m_client = new QAMQP::Client(this);
  m_client->open(m_url);
  m_client->setAutoReconnect(true);

  m_qIn = m_client->createQueue();
  m_qIn->declare(m_hostname + "_cmd", QAMQP::Queue::Durable);
  connect(m_qIn, &QAMQP::Queue::declared, this, [this] () {
      m_qIn->setQOS(0,1);
      m_qIn->consume();
    });
  connect(m_qIn, &QAMQP::Queue::messageReceived, this, &JobsExecuter::newCommand);

  m_exchange =  m_client->createExchange();

  m_qOut = m_client->createQueue(m_exchange->channelNumber());
  m_qOut->declare(m_hostname + "_rpl", QAMQP::Queue::Durable);

}

void JobsExecuter::newCommand()
{
  QAMQP::MessagePtr message = m_qIn->getMessage();
  qDebug() << "JobsExecuter::newMessage " << message->payload;

  QByteArray bMessage = message->payload;
  m_qIn->ack(message);

  QJsonParseError parseResult;
  auto doc = QJsonDocument::fromJson(bMessage, &parseResult);

  if (parseResult.error != QJsonParseError::NoError) {
      qDebug() << "Error parsing message: " + parseResult.errorString();
      return;
    }

  // we need { } style messages, ignore all other :)
  if (!doc.isObject()) {
      qDebug() << "Message is not an object: " + bMessage;
      return;
    }

  QVariantMap msg = doc.object().toVariantMap();

  QString type = msg.value("type").toString();
  QString jobid = msg.value("jobId").toString();

  if (type == "exec") {
      QString cmd = msg.value("cmd").toString();
      QStringList args = msg.value("args").toStringList();
      QByteArray data = QByteArray::fromBase64(msg.value("stdin").toByteArray());
      emit cmdExec(jobid, cmd, args, data);
    } else if (type == "terminate") {
      emit cmdTerminate(jobid);
    } else if (type == "kill") {
      emit cmdKill(jobid);
    }
}

void JobsExecuter::cmdExec(QString job, QString cmdline, QStringList args, QByteArray indata)
{
  if (m_pool.contains(job)) {
      qDebug() << "Duplicate jobid " + job;
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

  process->start(cmdline, args);
  if (!indata.isNull() && !indata.isEmpty()) {
      process->setProperty("stdin", indata);
    }
}

void JobsExecuter::cmdTerminate(QString job)
{
  if (!m_pool.contains(job)) {
      qDebug() << "Terminate of unknown job, jobid = " + job;
      return;
    }

  m_pool.value(job)->terminate();
}

void JobsExecuter::cmdKill(QString job)
{
  if (!m_pool.contains(job)) {
      qDebug() << "Kill of unknown job, jobid = " + job;
      return;
    }

  m_pool.value(job)->kill();
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
      m_pool.remove(process->property("job").toString());
      process->deleteLater();
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

void JobsExecuter::sendReply(QVariantMap reply)
{
  QAMQP::Exchange::MessageProperties properties;
  properties[QAMQP::Frame::Content::cpDeliveryMode] = 2; // Make message persistent
  m_exchange->publish(QJsonDocument::fromVariant(reply).toJson(), m_qOut->name(), properties);
  qDebug() << reply;
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

