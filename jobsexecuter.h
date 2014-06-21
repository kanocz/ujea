#ifndef JobsExecuter_H
#define JobsExecuter_H

#include <QObject>
#include <QUrl>
#include <QVariantMap>
#include <QMap>
#include <QProcess>

#include "qamqp/src/qamqp/amqp.h"
#include "qamqp/src/qamqp/amqp_queue.h"
#include "qamqp/src/qamqp/amqp_exchange.h"

class JobsExecuter : public QObject
{
  Q_OBJECT
public:
  explicit JobsExecuter(const QUrl& address, QString hostname, QObject *parent = 0);

signals:

public slots:

protected slots:

  void newCommand();

  void cmdExec(QString job, QString cmdline, QStringList args, QByteArray indata);
  void cmdTerminate(QString job);
  void cmdKill(QString job);

  void processError(QProcess::ProcessError error);
  void processFinised(int exitCode, QProcess::ExitStatus exitStatus);
  void processStarted();
  void processStdout();
  void processStderr();

private:
  QUrl m_url;
  QString m_hostname;

  QAMQP::Client* m_client;
  QAMQP::Queue* m_qIn;
  QAMQP::Queue* m_qOut;

  QAMQP::Exchange* m_exchange;
  QMap<QString, QProcess*> m_pool;

  void sendReply(QVariantMap reply);
  QVariantMap prepareReply(QString type, QString job = QString());
};

#endif // JobsExecuter_H
