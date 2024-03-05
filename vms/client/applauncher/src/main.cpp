// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QDir>
#include <QtCore/QCommandLineParser>
#include <QtCore/QStandardPaths>

#include <qtsinglecoreapplication.h>

#include <nx/utils/log/log_initializer.h>
#include <nx/utils/log/log.h>
#include <nx/utils/file_system.h>

#include <nx/branding.h>
#include <nx/build_info.h>

#include "applauncher_process.h"
#include "process_utils.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

using namespace nx::vms::applauncher;

#if defined(Q_OS_WIN)

static ApplauncherProcess* applauncherProcessInstance = nullptr;

static BOOL WINAPI stopServer_WIN(DWORD /*dwCtrlType*/)
{
    if (applauncherProcessInstance)
        applauncherProcessInstance->pleaseStop();
    return TRUE;
}

#endif // defined(Q_OS_WIN)

static void initLogFromArgs(const QString& logLevel, const QString& logFileBaseName)
{
    nx::log::Settings settings;
    settings.loggers.resize(1);
    settings.loggers.front().level.parse(logLevel);
    settings.loggers.front().maxFileSizeB = 1024 * 1024 * 10;
    settings.loggers.front().maxVolumeSizeB = 1024 * 1024 * 50;
    settings.loggers.front().logBaseName = logFileBaseName;

    nx::log::setMainLogger(
        nx::log::buildLogger(
            settings, nx::branding::applauncherInternalName()));
}

static bool initLogsFromConfigFile(const QString& logsDirectory)
{
    const QDir iniFilesDir(nx::kit::IniConfig::iniFilesDir());
    const QString logConfigFile(iniFilesDir.absoluteFilePath("applauncher_log.ini"));

    if (!QFile::exists(logConfigFile))
        return false;

    return nx::log::initializeFromConfigFile(
        logConfigFile,
        logsDirectory,
        qApp->applicationName(),
        qApp->applicationFilePath());
}

QStringList getClientArguments(QStringList arguments)
{
    // Application name goes first, we do not need to pass it furher.
    if (arguments.size() > 0)
        arguments.removeFirst();

    arguments.removeAll("--quit");
    arguments.removeAll("--exec");

    // Full cleanup implementation will require to reimplement command line parser, so let it be as
    // is for now. No side effects are expected.

    return arguments;
}

ApplauncherProcess::StartupParameters parseCommandLineParameters()
{
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption logLevelOption("log-level", "Log level.", "level");
    parser.addOption(logLevelOption);

    QCommandLineOption logFileBaseNameOption("log-file", "Log file.", "name");
    parser.addOption(logFileBaseNameOption);

    QCommandLineOption quitOption("quit", "Kill client application");
    parser.addOption(quitOption);

    QCommandLineOption execOption("exec", "Go to background");
    parser.addOption(execOption);

    QCommandLineOption protoVersionOption("proto", "Launch latest client with this protocol.",
        "protocol");
    parser.addOption(protoVersionOption);

    QStringList arguments = qApp->arguments();
    // Won't stop on cmdline parsing errors, to pass down additional params.
    const bool parsed = parser.parse(arguments);

    const QDir dataDirectory(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    nx::utils::file_system::ensureDir(dataDirectory);

    const QString logLevel = parser.value(logLevelOption);
    const QString logFileBaseName = parser.value(logFileBaseNameOption);

    if (!logFileBaseName.isEmpty() && !logLevel.isEmpty())
        initLogFromArgs(logLevel, logFileBaseName);
    else if (!initLogsFromConfigFile(dataDirectory.absolutePath() + "/log"))
        initLogFromArgs("warning", dataDirectory.absoluteFilePath("log/applauncher"));

    if (!parsed)
        NX_DEBUG(NX_SCOPE_TAG, "Cmdline parsing error: %1", parser.errorText());

    ApplauncherProcess::StartupParameters startupParameters;
    if (parser.isSet(quitOption))
        startupParameters.mode = ApplauncherProcess::StartupParameters::Mode::Quit;
    else if (parser.isSet(execOption))
        startupParameters.mode = ApplauncherProcess::StartupParameters::Mode::Background;
    startupParameters.targetProtoVersion = parser.value(protoVersionOption).toInt();
    startupParameters.clientCommandLineParameters = getClientArguments(arguments);

    return startupParameters;
}

int main(int argc, char* argv[])
{
    #ifdef Q_OS_UNIX
        // Prevent holding inherited file descriptors.
        // Otherwise if the applauncher is started via QProcess::startDetached() from the desktop
        // client the QtWebEngineProcess will outlive the desktop client.
        const int fdLimit = sysconf(_SC_OPEN_MAX);
        for (int i = STDERR_FILENO + 1; i < fdLimit; i++)
            close(i);
    #endif

    qApp->setOrganizationName(nx::branding::company());
    qApp->setApplicationName(nx::branding::applauncherInternalName());
    qApp->setApplicationVersion(nx::build_info::vmsVersion());

    QnLongRunnablePool runnablePool;

    QtSingleCoreApplication app(nx::branding::applauncherInternalName(), argc, argv);
    auto appDirPath = QCoreApplication::applicationDirPath();
    QDir::setCurrent(appDirPath);

    QSettings userSettings(
        QSettings::UserScope,
        nx::branding::company(),
        nx::branding::applauncherInternalName());

    ProcessUtils::initialize();

    const auto startupParameters = parseCommandLineParameters();

    InstallationManager installationManager;

    QScopedPointer<nx::utils::TimerManager> timerManager(new nx::utils::TimerManager());
    ApplauncherProcess applauncherProcess(
        &userSettings,
        &installationManager,
        startupParameters);

    #if defined(Q_OS_WIN)
        applauncherProcessInstance = &applauncherProcess;
        SetConsoleCtrlHandler(stopServer_WIN, true);
    #endif

    applauncherProcess.initChannels();
    int status = applauncherProcess.run();

    #if defined(Q_OS_WIN)
        if (startupParameters.mode == ApplauncherProcess::StartupParameters::Mode::Quit)
        {
            // Wait for app to finish + 100ms just in case.
            // It may be still running after unlocking QSingleApplication lock file.
            while (app.isRunning())
            {
                Sleep(100);
            }
            Sleep(100);
        }
    #endif

    return status;
}
