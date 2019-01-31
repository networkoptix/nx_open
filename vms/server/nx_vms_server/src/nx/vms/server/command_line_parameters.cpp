#include "command_line_parameters.h"

#include <utils/common/command_line_parser.h>
#include <nx/utils/log/log_level.h>
#include <nx/utils/app_info.h>
#include <nx/utils/argument_parser.h>

#include <utils/common/app_info.h>
#include <media_server/settings.h>

namespace {

QString defaultRunTimeSettingsFilePath()
{
    if (QnAppInfo::isWindows())
        return "windows registry is used";
    else
        return MSSettings::defaultConfigFileNameRunTime;
}

QString defaultROSettingsFilePath()
{
    if (QnAppInfo::isWindows())
        return "windows registry is used";
    else
        return MSSettings::defaultConfigFileName;
}

} // namespace

namespace nx {
namespace vms::server {

CmdLineArguments::CmdLineArguments(const QStringList& arguments)
{
    init(arguments);
}

CmdLineArguments::CmdLineArguments(int argc, char* argv[])
{
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments << QString::fromUtf8(argv[i]);
    init(arguments);
}

void CmdLineArguments::init(const QStringList& arguments)
{
    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&logLevel, "--log-level", NULL,
        lit("Supported values: none (no logging), always, error, warning, info, debug, verbose. Default: ")
        + toString(nx::utils::log::kDefaultLevel));

    commandLineParser.addParameter(&httpLogLevel, "--http-log-level", NULL, "Log value for http_log.log.");
    commandLineParser.addParameter(&systemLogLevel, "--system-log-level", NULL, "Log value for hw_log.log.");
    commandLineParser.addParameter(&ec2TranLogLevel, "--ec2-tran-log-level", NULL, "Log value for ec2_tran.log.");
    commandLineParser.addParameter(&permissionsLogLevel, "--permissions-log-level", NULL, "Log value for permissions.log.");

    commandLineParser.addParameter(&rebuildArchive, "--rebuild", NULL,
        lit("Rebuild archive index. Supported values: all (high & low quality), hq (only high), lq (only low)"), "all");
    commandLineParser.addParameter(&allowedDiscoveryPeers, "--allowed-peers", NULL, QString());
    commandLineParser.addParameter(&ifListFilter, "--if", NULL,
        "Strict media server network interface list (comma delimited list)");
    commandLineParser.addParameter(&configFilePath, "--conf-file", NULL,
        "Path to config file. By default " + defaultROSettingsFilePath());
    commandLineParser.addParameter(&rwConfigFilePath, "--runtime-conf-file", NULL,
        "Path to config file which is used to save some. By default " + defaultRunTimeSettingsFilePath());
    commandLineParser.addParameter(&showVersion, "--version", NULL,
        lit("Print version info and exit"), true);
    commandLineParser.addParameter(&showHelp, "--help", NULL,
        lit("This help message"), true);
    commandLineParser.addParameter(&engineVersion, "--override-version", NULL,
        lit("Force the other engine version"), QString());
    commandLineParser.addParameter(&enforceSocketType, "--enforce-socket", NULL,
        lit("Enforces stream socket type (TCP, UDT)"), QString());
    commandLineParser.addParameter(&enforcedMediatorEndpoint, "--enforce-mediator", NULL,
        lit("Enforces mediator address"), QString());
    commandLineParser.addParameter(&ipVersion, "--ip-version", NULL,
        lit("Force ip version"), QString());
    commandLineParser.addParameter(&createFakeData, "--create-fake-data", NULL,
        lit("Create fake data: users,cameras,propertiesPerCamera,camerasPerLayout,storageCount"), QString());
    commandLineParser.addParameter(&crashDirectory, "--crash-directory", NULL,
        lit("Directory to save and send crash reports."), QString());
    commandLineParser.addParameter(&cleanupDb, "--cleanup-db", NULL,
        lit("Deletes resources with NULL ids, "
            "cleans dangling cameras' and servers' user attributes, "
            "kvpairs and resourceStatuses, also cleans and rebuilds transaction log"), true);

    commandLineParser.addParameter(&moveHandlingCameras, "--move-handling-cameras", NULL,
        lit("Move handling cameras to itself, "
            "In some rare scenarios cameras can be assigned to removed server, "
            "This startup parameter force server to move these cameras to itself"), true);

    commandLineParser.addParameter(&auxLoggers, "--log/logger", NULL,
        lit("Additional logger configuration. "
            "E.g., to log every message <= WARING to stdout: file=-;level=WARNING"));

    commandLineParser.parse(arguments, stderr);
    if (showHelp)
    {
        QTextStream stream(stdout);
        commandLineParser.print(stream);
    }
    if (showVersion)
        std::cout << nx::utils::AppInfo::applicationFullVersion().toStdString() << std::endl;

    // NOTE: commandLineParser does not support multiple arguments with the same name.
    const nx::utils::ArgumentParser argumentParser(arguments);
    argumentParser.forEach(
        "log/logger",
        [this](const QString& value) { auxLoggers.push_back(value); });
}

} // namespace vms::server
} // namespace nx
