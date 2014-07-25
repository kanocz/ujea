#include <QCoreApplication>
#include <QCommandLineParser>
#include <QHostInfo>

#include "jobsexecuter.h"

int main(int argc, char *argv[])
{
  QCoreApplication app(argc, argv);

  // some command line arguments parsing stuff
  QCoreApplication::setApplicationName("ujea");
  QCoreApplication::setApplicationVersion("1.2");

  QCommandLineParser parser;
  parser.setApplicationDescription("Universal job execution agent using AMQP queue");
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addOption(QCommandLineOption("aliveInterval", "Interval in ms between alive messages (1000).", "aliveInterval", "1000"));
  parser.addOption(QCommandLineOption("aliveTTL", "TTL in queue for alive messages (1500).", "aliveTTL", "1500"));
  parser.addPositionalArgument("url", QCoreApplication::translate("main", "AMQP url"));
  parser.process(app);

  const QStringList args = parser.positionalArguments();

  // we need to have 1 argument and optional named arguments
  if (args.count() != 1) parser.showHelp(-1);

  // create and execure worker
  JobsExecuter qw{QUrl(args.value(0)), QHostInfo::localHostName(),
              parser.value("aliveInterval").toInt(), parser.value("aliveTTL").toInt()};

  return app.exec();
}
