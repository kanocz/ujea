#ifndef JobsExecuter_H
#define JobsExecuter_H

#include <QObject>
#include <QUrl>
#include <QVariantMap>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>
#include <QTemporaryDir>

#include "qamqp/src/qamqp/amqp.h"
#include "qamqp/src/qamqp/amqp_queue.h"
#include "qamqp/src/qamqp/amqp_exchange.h"

class JobsExecuter : public QObject
{
  Q_OBJECT
public:
  explicit JobsExecuter(const QUrl& address, QString q_cmd, QString q_rpl,
                        int aliveInterval, int aliveTTL, QString execRegex,
                        QObject *parent = 0);

signals:

public slots:

protected slots:

  void newCommand();

  void cmdExec(QString job, QString cmdline, QStringList args,
               QByteArray indata, QVariantList env, QByteArray tempfile);
  void cmdTerminate(QString job);
  void cmdKill(QString job);
  void cmdStdin(QString job, QByteArray indata);
  void cmdClose(QString job);
  void cmdError(QString job, QString error);

  void processError(QProcess::ProcessError error);
  void processFinised(int exitCode, QProcess::ExitStatus exitStatus);
  void processStarted();
  void processStdout();
  void processStderr();

  void sendAlive();

private:
  QUrl m_url;
  int m_aliveTTL;

  QAMQP::Client* m_client;
  QAMQP::Queue* m_qIn;
  QAMQP::Queue* m_qOut;

  QAMQP::Exchange* m_exchange;
  QMap<QString, QProcess*> m_pool;

  QTimer aliveTimer;

  void sendReply(QVariantMap reply);
  QVariantMap prepareReply(QString type, QString job = QString());

  QTemporaryDir tempdir;
  QRegularExpression execRe;
};

#endif // JobsExecuter_H
