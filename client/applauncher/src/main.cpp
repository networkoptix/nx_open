#include <iomanip>
#include <iostream>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>

#include <qtsinglecoreapplication.h>

#include <utils/common/command_line_parser.h>
#include <utils/common/app_info.h>

#include <nx/utils/log/log_initializer.h>
#include <nx/utils/log/log.h>

#include <applauncher_app_info.h>
#include "applauncher_process.h"
#include "process_utils.h"

#ifdef _WIN32
#include <windows.h>

static ApplauncherProcess* applauncherProcessInstance = 0;

static BOOL WINAPI stopServer_WIN(DWORD /*dwCtrlType*/)
{
    if (applauncherProcessInstance)
        applauncherProcessInstance->pleaseStop();
    return TRUE;
}
#endif

ApplauncherProcess::StartupParameters parseCommandLineParameters(int argc, char* argv[])
{
    QString installationsDir = InstallationManager::defaultDirectoryForInstallations();
    if (!QDir(installationsDir).exists())
        QDir().mkpath(installationsDir);

    QString logLevel = "WARN";
    QString logFilePath = installationsDir + "/applauncher";
    bool quitMode = false;
    bool backgroundMode = false;
    QString version;

    ApplauncherProcess::StartupParameters startupParameters;

    QnCommandLineParser commandLineParser;
    commandLineParser.storeUnparsed(&startupParameters.clientCommandLineParameters);
    commandLineParser.addParameter(&logLevel, "--log-level", QString(), QString());
    commandLineParser.addParameter(&logFilePath, "--log-file", QString(), QString());
    commandLineParser.addParameter(&quitMode, "--quit", QString(), QString(), true);
    commandLineParser.addParameter(&backgroundMode, "--exec", QString(), QString(), true);
    commandLineParser.addParameter(&startupParameters.targetProtoVersion, "--proto",
        QString(), QString(), 0);
    commandLineParser.addParameter(&version, "--version", QString(), QString(), 0);
    commandLineParser.parse(argc, (const char**)argv, stderr);

    //initialize logging based on args
    if (!logFilePath.isEmpty() && !logLevel.isEmpty())
    {
        nx::utils::log::Settings settings;
        settings.loggers.resize(1);
        settings.loggers.front().level.parse(logLevel);
        settings.loggers.front().maxFileSize = 1024 * 1024 * 10;
        settings.loggers.front().maxBackupCount = 5;
        settings.loggers.front().logBaseName = logFilePath;

        nx::utils::log::setMainLogger(
            nx::utils::log::buildLogger(
                settings, QnApplauncherAppInfo::applicationName()));
    }

    if (quitMode)
        startupParameters.mode = ApplauncherProcess::StartupParameters::Mode::Quit;
    else if (backgroundMode)
        startupParameters.mode = ApplauncherProcess::StartupParameters::Mode::Background;

    return startupParameters;
}

int main(int argc, char* argv[])
{
    qApp->setOrganizationName(QnAppInfo::organizationName());
    qApp->setApplicationName(QnApplauncherAppInfo::applicationName());
    qApp->setApplicationVersion(QnAppInfo::applicationVersion());

    QnLongRunnablePool runnablePool;

    QtSingleCoreApplication app(QnApplauncherAppInfo::applicationName(), argc, argv);
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    QSettings userSettings(
        QSettings::UserScope,
        QnAppInfo::organizationName(),
        QnApplauncherAppInfo::applicationName());

    ProcessUtils::initialize();

    const auto startupParameters = parseCommandLineParameters(argc, argv);

    InstallationManager installationManager;

    QScopedPointer<nx::utils::TimerManager> timerManager(new nx::utils::TimerManager());
    ApplauncherProcess applauncherProcess(
        &userSettings,
        &installationManager,
        startupParameters);

#ifdef _WIN32
    applauncherProcessInstance = &applauncherProcess;
    SetConsoleCtrlHandler(stopServer_WIN, true);
#endif

    int status = applauncherProcess.run();

#ifdef _WIN32
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
