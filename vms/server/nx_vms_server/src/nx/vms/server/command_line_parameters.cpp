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
    commandLineParser.addParameter(&logLevel, "--log-level", nullptr,
        "Supported values: none (no logging), always, error, warning, info, debug, verbose. "
        "Default: " + toString(nx::utils::log::kDefaultLevel));

    commandLineParser.addParameter(&httpLogLevel, "--http-log-level", nullptr,
        "Log value for http_log.log.");
    commandLineParser.addParameter(&systemLogLevel, "--system-log-level", nullptr,
        "Log value for hw_log.log.");
    commandLineParser.addParameter(&ec2TranLogLevel, "--ec2-tran-log-level", nullptr,
        "Log value for ec2_tran.log.");
    commandLineParser.addParameter(&permissionsLogLevel, "--permissions-log-level", nullptr,
        "Log value for permissions.log.");

    commandLineParser.addParameter(&rebuildArchive, "--rebuild", nullptr,
        "Rebuild archive index. Supported values: "
        "all (high & low quality), hq (only high), lq (only low),",
        "all");
    commandLineParser.addParameter(&allowedDiscoveryPeers, "--allowed-peers", nullptr, "");
    commandLineParser.addParameter(&ifListFilter, "--if", nullptr,
        "Strict Server network interfaces (a comma-delimited list).");
    commandLineParser.addParameter(&configFilePath, "--conf-file", nullptr,
        "Path to config file. Default: " + defaultROSettingsFilePath());
    commandLineParser.addParameter(&rwConfigFilePath, "--runtime-conf-file", nullptr,
        "Path to config file which is used to save some. Default: "
        + defaultRunTimeSettingsFilePath());
    commandLineParser.addParameter(&showVersion, "--version", nullptr,
        "Print version info and exit.", true);
    commandLineParser.addParameter(&showHelp, "--help", nullptr,
        "Show this help message.", true);
    commandLineParser.addParameter(&engineVersion, "--override-version", nullptr,
        "Force the other engine version.", "");
    commandLineParser.addParameter(&vmsProtocolVersion, "--override-protocol-version", nullptr,
        "Force VMS protocol version.", "");
    commandLineParser.addParameter(&enforceSocketType, "--enforce-socket", nullptr,
        "Enforce stream socket type: TCP, UDT.", "");
    commandLineParser.addParameter(&enforcedMediatorEndpoint, "--enforce-mediator", nullptr,
        "Enforce mediator address.", "");
    commandLineParser.addParameter(&ipVersion, "--ip-version", nullptr,
        "Force ip version.", "");
    commandLineParser.addParameter(&createFakeData, "--create-fake-data", nullptr,
        "Create fake data: users, cameras, propertiesPerCamera, camerasPerLayout, storageCount.",
        "");
    commandLineParser.addParameter(&crashDirectory, "--crash-directory", nullptr,
        "Directory to save and send crash reports.", "");
    commandLineParser.addParameter(&cleanupDb, "--cleanup-db", nullptr,
        "Deletes resources with nullptr ids, "
        "cleans dangling cameras' and Servers' user attributes, "
        "kvpairs and resourceStatuses, also cleans and rebuilds transaction log",
        true);

    commandLineParser.addParameter(&moveHandlingCameras, "--move-handling-cameras", nullptr,
        "Move handling cameras to this Server. "
        "In some rare scenarios cameras can be assigned to a removed Server. "
        "This startup parameter forces the Server to move these cameras to itself.",
        true);

    commandLineParser.addParameter(&auxLoggers, "--log/logger", nullptr,
        "Additional logger configuration. "
        "E.g., to log every message <= WARNING to stdout: file=-;level=WARNING");

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
