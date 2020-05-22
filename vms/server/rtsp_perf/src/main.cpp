#include <nx/utils/log/log.h>

#include <optional>

#include <QtCore/QCommandLineParser>

#include <nx/kit/output_redirector.h>
#include <nx/utils/app_info.h>

#include "rtsp_perf.h"
#include "rtsp_perf_ini.h"

namespace {

template<typename... Args>
void print(std::ostream& stream, const QString& message, Args... args)
{
    stream << nx::utils::log::makeMessage(message + "\n", args...).toStdString();
}

static bool validateConfig(const RtspPerf::Config& config)
{
    if (config.count < 1)
    {
        print(std::cerr, "ERROR: At least one session needed, use '-c <count>' option.");
        return false;
    }
    if (config.livePercent > 100 || config.livePercent < 0)
    {
        print(std::cerr,
            "ERROR: Percentage of live streams must be [0..100], use '-c <count>' option.");
        return false;
    }
    if (config.server.isEmpty())
    {
        print(std::cerr, "ERROR: Wrong server url, use '-u <url>' option.");
        return false;
    }
    return true;
}

static std::optional<RtspPerf::Config> makeConfig(const QCoreApplication& app)
{
    QCommandLineParser parser;

    parser.setApplicationDescription("Rtsp performance test");
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
        "Percentage of live streams. By default: 100", "live percent", "100");
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
        "Log level: one of NONE, ERROR, WARNING, INFO, DEBUG, VERBOSE.", "level", "");
    parser.addOption(logLevelOption);

    QCommandLineOption disableRestart(QStringList() << "disable-restart",
        "Start every RTSP session only once.");
    parser.addOption(disableRestart);

    QCommandLineOption maxTimestampDiffUs(QStringList() << "max-timestamp-diff-us",
        "Max timestamp diff in microseconds.", "max-diff", "0");
    parser.addOption(maxTimestampDiffUs);

    QCommandLineOption minTimestampDiffUs(QStringList() << "min-timestamp-diff-us",
        "Min timestamp diff in microseconds.", "min-diff", "0");
    parser.addOption(minTimestampDiffUs);

    QCommandLineOption ignoreSequenceNumberErrors(QStringList() << "ignore-sequence-number-errors",
        "Disable sequential number error warnings");
    parser.addOption(ignoreSequenceNumberErrors);

    QCommandLineOption archivePositionOption(QStringList() << "archive-position",
        "Position in utc msec for all archive sessions", "archive-position", "-1");
    parser.addOption(archivePositionOption);

    parser.process(app);

    RtspPerf::Config config;
    config.count = parser.value(countOption).toInt();
    config.startInterval = std::chrono::milliseconds(parser.value(intervalOption).toInt());
    config.livePercent = parser.value(livePercentOption).toInt();
    config.server = parser.value(serverOption);
    config.useSsl = parser.isSet(sslOption);
    config.urls = parser.values(urlOption);
    config.disableRestart = parser.isSet(disableRestart);

    config.sessionConfig.timeout = std::chrono::milliseconds(parser.value(timeoutOption).toInt());
    config.sessionConfig.user = parser.value(userOption);
    config.sessionConfig.password = parser.value(passwordOption);
    config.sessionConfig.printTimestamps = parser.isSet(timestampsOption);
    config.sessionConfig.ignoreSequenceNumberErrors = parser.isSet(ignoreSequenceNumberErrors);
    config.sessionConfig.maxTimestampDiff =
        std::chrono::microseconds(parser.value(maxTimestampDiffUs).toLongLong());
    config.sessionConfig.minTimestampDiff =
        std::chrono::microseconds(parser.value(minTimestampDiffUs).toLongLong());
    config.sessionConfig.archivePosition =
        std::chrono::milliseconds(parser.value(archivePositionOption).toLongLong());

    if (parser.isSet(logLevelOption))
    {
        const QString logLevel = parser.value(logLevelOption);
        const auto level = nx::utils::log::levelFromString(logLevel);
        if (level == nx::utils::log::Level::undefined)
        {
            print(std::cerr, "ERROR: Invalid log level: %1", logLevel);
            return std::nullopt;
        }
        nx::utils::log::mainLogger()->setDefaultLevel(level);
    }

    if (parser.isSet(serverOption) && !config.urls.isEmpty())
        print(std::cerr, "WARNING: Url configured, '--server' option will be ignored.");

    return config;
}

static int run(int argc, char* argv[], const QCoreApplication& app)
{
    const std::optional<RtspPerf::Config> config = makeConfig(app);
    if (!config || !validateConfig(*config))
        return 1;

    RtspPerf perf(*config);
    perf.run();
    return 1; //< Fatal error, because normally RtspPerf::run() runs an infinite loop.
}

} // namespace

int main(int argc, char** argv)
{
    int exitStatus;
    try
    {
        nx::kit::OutputRedirector::ensureOutputRedirection();
        nx::utils::log::setLevelReducerEnabled(false);

        ini().reload(); //< Make .ini appear on the console even when help is requested.

        QCoreApplication::setOrganizationName(nx::utils::AppInfo::organizationName());
        QCoreApplication::setApplicationName(nx::utils::AppInfo::vmsName() + " RtspPerf");
        QCoreApplication::setApplicationVersion(nx::utils::AppInfo::applicationVersion());
        const QCoreApplication app(argc, argv); //< Each user may run their own executable.

        // NOTE: Print blank lines before (after ini-config printout) and after to emphasize.
        print(std::cout, "\n%1 version %2\n", app.applicationName(), app.applicationVersion());

        exitStatus = run(argc, argv, app);
    }
    catch (const std::exception& e)
    {
        print(std::cerr, "INTERNAL ERROR: Exception raised: %1", e.what());
        exitStatus = 70;
    }
    catch (...)
    {
        print(std::cerr, "INTERNAL ERROR: Unknown exception raised.");
        exitStatus = 71;
    }
    print(std::cerr, "Finished with exit status %1.", exitStatus);
    return exitStatus;
}
