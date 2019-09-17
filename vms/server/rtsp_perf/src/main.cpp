#include "rtsp_perf.h"
#include <nx/utils/log/log.h>

#include <QtCore/QCommandLineParser>

bool validateConfig(RtspPerf::Config& config)
{
    if (config.count < 1)
    {
        fprintf(stderr, "ERROR: At least one session needed, use '-c <count>' option\n");
        return false;
    }
    if (config.livePercent > 100 || config.livePercent < 0)
    {
        fprintf(stderr, "ERROR: "
            "Live persent streams should be in interval [0..100], use '-c <count>' option\n");
        return false;
    }
    if (config.server.isEmpty())
    {
        fprintf(stderr, "ERROR: Wrong server url, use '-u <url>' option\n");
        return false;
    }
    return true;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("RtspPerf");
    QCoreApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Rtsp perfomance test");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption serverOption(QStringList() << "s" << "server",
        "Server address and port. By default: '127.0.0.1:7001'.", "server address",
        "127.0.0.1:7001");
    parser.addOption(serverOption);
    QCommandLineOption countOption(QStringList() << "c" << "count",
        "Rtsp session count. By default: 1", "count", "1");
    parser.addOption(countOption);
    QCommandLineOption livePercentOption(QStringList() << "l" << "live_percent",
        "Percent of live streams. By default: 100", "live percent", "100");
    parser.addOption(livePercentOption);
    QCommandLineOption timeoutOption(QStringList() << "t" << "timeout",
        "RTSP read timeout in milliseconds. By default: 5000ms", "timeout", "5000");
    parser.addOption(timeoutOption);
    QCommandLineOption intervalOption(QStringList() << "i" << "interval",
        "Session start interval in milliseconds, if 0 than auto mode will used, that increase "
        "interval two times (from 100ms up to 10 seconds) on every start session fail. "
        "By default: 0ms that mean auto", "interval", "0");
    parser.addOption(intervalOption);
    QCommandLineOption userOption(QStringList() << "u" << "user",
        "Server user name. By default: 'admin'", "user", "admin");
    parser.addOption(userOption);
    QCommandLineOption passwordOption(QStringList() << "p" << "password",
        "Server user password", "password", "");
    parser.addOption(passwordOption);
    QCommandLineOption sslOption(QStringList() << "ssl",
        "Use SSL. By default: 'false'");
    parser.addOption(sslOption);
    QCommandLineOption timestampsOption(QStringList() << "timestamps",
        "Print frame timestamps. By default: 'false'");
    parser.addOption(timestampsOption);
    QCommandLineOption urlOption(QStringList() << "url",
        "Force camera URL, all sessions will use this URL to connect to the Server, option "
        "'--server' will be ignored. Repeat to set multiple URLs.", "url", "");
    parser.addOption(urlOption);
    QCommandLineOption logLevelOption(QStringList() << "log-level",
        "Log level(NONE, ERROR, WARNING, INFO, DEBUG, VERBOSE)", "level", "");
    parser.addOption(logLevelOption);
    parser.process(app);


    RtspPerf::Config config;
    config.count = parser.value(countOption).toInt();
    config.startInterval = std::chrono::milliseconds(parser.value(intervalOption).toInt());
    config.timeout = std::chrono::milliseconds(parser.value(timeoutOption).toInt());
    config.livePercent = parser.value(livePercentOption).toInt();
    config.server = parser.value(serverOption);
    config.user = parser.value(userOption);
    config.password = parser.value(passwordOption);
    config.useSsl = parser.isSet(sslOption);
    config.printTimestamps = parser.isSet(timestampsOption);
    config.urls = parser.values(urlOption);

    if (parser.isSet(logLevelOption))
    {
        const QString logLevel = parser.value(logLevelOption);
        const auto level = nx::utils::log::levelFromString(logLevel);
        if (level == nx::utils::log::Level::undefined)
        {
            fprintf(stderr, "ERROR: Invalid log level: %s\n", logLevel.toUtf8().data());
            return 1;
        }
        nx::utils::log::mainLogger()->setDefaultLevel(level);
    }

    if (parser.isSet(serverOption) && !config.urls.isEmpty())
        fprintf(stderr, "WARNING: Url configured, '--server' option will ignored\n");

    if (!validateConfig(config))
        return 1;

    RtspPerf perf(config);
    perf.run();
    return 0;
}
