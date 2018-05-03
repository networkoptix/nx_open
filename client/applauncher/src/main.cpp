////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include <iomanip>
#include <iostream>

#include <qtsinglecoreapplication.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QThread>

#include <utils/common/command_line_parser.h>
#include <nx/utils/log/log_initializer.h>
#include <utils/common/app_info.h>
#include <nx/network/socket_global.h>

#include <applauncher_app_info.h>
#include "applauncher_process.h"
#include "installation_process.h"
#include "rdir_syncher.h"
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

static void printHelp()
{
    std::cout <<
        "Arguments:\n"
        "  Default mode (launches compatibility client)\n"
        "    --log-level={log level}. Can be ERROR, WARN, INFO, DEBUG, DEBUG2. By default, WARN\n"
        "    --log-file=/path/to/log/file. If not specified, no log is written\n"
        "    --mirror-list-url={url to mirrorList.xml} Overrides url found in conf file\n"
        "    quit. Tells another running launcher to terminate\n"
        "\n"
        "  Directory synchronization mode\n"
        "    --rsync Enable directory synchronization mode\n"
        "    --dir={local path} Path to local directory to sync\n"
        "    --url={http url} Url of remote http directory to sync with\n"
        "\n"
        "  Background mode\n"
        "    --exec Run in background mode\n"
        "\n"
        "  Installation mode (installs requested version)\n"
        "    --install Run in installation mode (install version specified by following parameters)\n"
        "    --version= Mandatory in installation mode\n"
        "    --product-name= Optional. By default, \"" << QnAppInfo::productNameShort().toUtf8().data() << "\"\n"
        "    --customization= Optional. By default, \"" << QnAppInfo::customizationName().toUtf8().data() << "\"\n"
        "    --module= Optional. By default, \"client\"\n"
        "    --install-path= Optional. By default, " << InstallationManager::defaultDirectoryForInstallations().toStdString() << "/{version}\n"
        "\n"
        "  --help. This help message\n"
        "\n";
}

int syncDir(const QString& localDir, QString remoteUrl);
int doInstallation(
    const InstallationManager& installationManager,
    const QString& mirrorListUrl,
    const QString& productName,
    const QString& customization,
    const QnSoftwareVersion &version,
    const QString& module,
    const QString& installationPath);

int downloadFile(const QString& url, const QString& destFilePath);

int main(int argc, char* argv[])
{
    nx::network::SocketGlobals::init();

    QnLongRunnablePool runnablePool;

    QString logLevel = "warning";
    QString installationsDir = InstallationManager::defaultDirectoryForInstallations();
    if (!QDir(installationsDir).exists())
        QDir().mkpath(installationsDir);
    QString logFilePath = installationsDir + "/applauncher";
    QString mirrorListUrl;
    bool quitMode = false;
    bool displayHelp = false;

    bool syncMode = false;
    bool backgroundMode = false;
    QString localDir;
    QString remoteUrl;
    bool installMode = false;
    QString versionToInstall;
    QString productNameToInstall(QnAppInfo::productNameShort());
    QString customizationToInstall(QnAppInfo::customizationName());
    QString moduleToInstall(QString::fromLatin1("client"));
    QString installationPath;
    QString devModeKey;

    QString fileToDownload;
    QString destFilePath;

    QStringList applicationParameters;

    QnCommandLineParser commandLineParser;
    commandLineParser.storeUnparsed(&applicationParameters);
    commandLineParser.addParameter(&logLevel, "--log-level", NULL, QString());
    commandLineParser.addParameter(&logFilePath, "--log-file", NULL, QString());
    commandLineParser.addParameter(&mirrorListUrl, "--mirror-list-url", NULL, QString());
    commandLineParser.addParameter(&quitMode, "quit", NULL, QString(), QVariant(true));
    commandLineParser.addParameter(&displayHelp, "--help", NULL, QString(), QVariant(true));
    commandLineParser.addParameter(&syncMode, "--rsync", NULL, QString(), QVariant(true));
    commandLineParser.addParameter(&backgroundMode, "--exec", NULL, QString(), QVariant(true));
    commandLineParser.addParameter(&localDir, "--dir", NULL, QString(), QString());
    commandLineParser.addParameter(&remoteUrl, "--url", NULL, QString(), QString());
    commandLineParser.addParameter(&installMode, "--install", NULL, QString(), QVariant(true));
    commandLineParser.addParameter(&versionToInstall, "--version", NULL, QString(), QString());
    commandLineParser.addParameter(&productNameToInstall, "--product-name", NULL, QString(), QString());
    commandLineParser.addParameter(&customizationToInstall, "--customization", NULL, QString(), QString());
    commandLineParser.addParameter(&moduleToInstall, "--module", NULL, QString(), QString());
    commandLineParser.addParameter(&installationPath, "--install-path", NULL, QString(), QString());
    commandLineParser.addParameter(&devModeKey, "--dev-mode-key", NULL, QString(), QString());
    commandLineParser.addParameter(&fileToDownload, "--get", "-g", QString(), QString());
    commandLineParser.addParameter(&destFilePath, "--out-file", "-o", QString(), QString());
    commandLineParser.parse(argc, (const char**) argv, stderr);

    qApp->setOrganizationName(QnAppInfo::organizationName());
    qApp->setApplicationName(QnApplauncherAppInfo::applicationName());
    qApp->setApplicationVersion(QnAppInfo::applicationVersion());

    QtSingleCoreApplication app(QnApplauncherAppInfo::applicationName(), argc, argv);
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    QSettings globalSettings(QSettings::SystemScope, QnAppInfo::organizationName(), QnApplauncherAppInfo::applicationName());
    QSettings userSettings(QSettings::UserScope, QnAppInfo::organizationName(), QnApplauncherAppInfo::applicationName());

    if (mirrorListUrl.isEmpty())
        mirrorListUrl = globalSettings.value("mirrorListUrl", QnApplauncherAppInfo::mirrorListUrl()).toString();

    if (mirrorListUrl.isEmpty())
        NX_LOG("MirrorListUrl is empty", cl_logWARNING);

    if (displayHelp)
    {
        printHelp();
        return 0;
    }

    //initialize logging based on args
    if (!logFilePath.isEmpty() && !logLevel.isEmpty())
    {
        nx::utils::log::Settings settings;
        settings.level.parse(logLevel);
        settings.maxFileSize = 1024 * 1024 * 10;
        settings.maxBackupCount = 5;

        nx::utils::log::initialize(
            settings, QnApplauncherAppInfo::applicationName(), QString(), logFilePath);
    }

    InstallationManager installationManager;

    if (!fileToDownload.isEmpty())
        return downloadFile(fileToDownload, destFilePath);
    if (syncMode)
        return syncDir(localDir, remoteUrl);
    if (installMode)
        return doInstallation(
            installationManager,
            mirrorListUrl,
            productNameToInstall,
            customizationToInstall,
            QnSoftwareVersion(versionToInstall),
            moduleToInstall,
            installationPath);

    ProcessUtils::initialize();

    ApplauncherProcess::Mode mode = ApplauncherProcess::Mode::Default;
    if (quitMode)
        mode = ApplauncherProcess::Mode::Quit;
    else if (backgroundMode)
        mode = ApplauncherProcess::Mode::Background;

    QScopedPointer<nx::utils::TimerManager> timerManager(new nx::utils::TimerManager());
    ApplauncherProcess applauncherProcess(
        &userSettings,
        &installationManager,
        mode,
        applicationParameters,
        mirrorListUrl);

    applauncherProcess.setDevModeKey(devModeKey);

#ifdef _WIN32
    applauncherProcessInstance = &applauncherProcess;
    SetConsoleCtrlHandler(stopServer_WIN, true);
#endif

    int status = applauncherProcess.run();

#ifdef _WIN32
    if (quitMode)
    {
        // Wait for app to finish + 100ms just in case (in may be still running after unlocking QSingleApplication lock file).
        while (app.isRunning())
        {
            Sleep(100);
        }
        Sleep(100);
    }
#endif

    return status;
}

//--rsync --url=http://downloads.hdwitness.com/test/x64/ --dir=c:/tmp/appl/
//--install --customization=digitalwatchdog --version=1.5

int syncDir(const QString& localDir, QString remoteUrl)
{
    if (localDir.isEmpty())
    {
        std::cerr << "Error: No target directory specified" << std::endl;
        return 1;
    }
    if (remoteUrl.isEmpty())
    {
        std::cerr << "Error: No remote url specified" << std::endl;
        return 1;
    }
    remoteUrl = lit("http://") + remoteUrl;

    RDirSyncher::EventReceiver dummyEventReceiver;
    auto syncher = std::make_shared<RDirSyncher>(nx::utils::Url(remoteUrl), localDir, &dummyEventReceiver);
    if (!syncher->startAsync())
    {
        std::cerr << "Error: Failed to start synchronization" << std::endl;
        return 2;
    }

    while (syncher->state() == RDirSyncher::sInProgress)
        QThread::msleep(1000);

    switch (syncher->state())
    {
        case RDirSyncher::sSucceeded:
            std::cout << "Synchronization finished" << std::endl;
            return 0;

        case RDirSyncher::sInterrupted:
            std::cerr << "Synchronization has been interrupted" << std::endl;
            return 3;

        case RDirSyncher::sFailed:
            std::cerr << "Synchronization has failed: " << syncher->lastErrorText().toStdString() << std::endl;
            return 3;

        default:
            std::cerr << "Synchronization ended in unknown state " << static_cast<int>(syncher->state()) << std::endl;
            return 3;
    }
}

int doInstallation(
    const InstallationManager& installationManager,
    const QString& mirrorListUrl,
    const QString& productName,
    const QString& customization,
    const QnSoftwareVersion& version,
    const QString& module,
    const QString& installationPath)
{
    if (version.isNull())
    {
        std::cerr << "FAILURE. Missing required parameter \"version\"" << std::endl;
        return 1;
    }

    QString effectiveInstallationPath = installationPath;
    if (effectiveInstallationPath.isEmpty())
        effectiveInstallationPath = installationManager.installationDirForVersion(version);

    InstallationProcess installationProcess(
        productName,
        customization,
        version,
        module,
        effectiveInstallationPath,
        false);
    if (!installationProcess.start(mirrorListUrl))
    {
        std::cerr << "FAILURE. Cannot start installation. " << installationProcess.errorText().toStdString() << std::endl;
        return 1;
    }

    while (installationProcess.getStatus() <= applauncher::api::InstallationStatus::inProgress)
    {
        static const unsigned int INSTALLATION_STATUS_CHECK_TIMEOUT_MS = 1000;
        QThread::msleep(INSTALLATION_STATUS_CHECK_TIMEOUT_MS);
        std::cout << "Current progress: " << std::fixed << std::setprecision(2) << installationProcess.getProgress() << "%" << std::endl;
    }

    switch (installationProcess.getStatus())
    {
        case applauncher::api::InstallationStatus::success:
            std::cout << "SUCCESS" << std::endl;
            break;

        case applauncher::api::InstallationStatus::failed:
            std::cerr << "FAILURE. " << installationProcess.errorText().toStdString() << std::endl;
            break;

        default:
            NX_ASSERT(false);
    }

    return 0;
}


#include <fstream>
#include <string.h>

#include <nx/network/http/http_client.h>

//--rsync --dir=c:/temp/1 --url=enk.me/clients/2.1/default/windows/x64/
//--rsync --dir=c:/tmp/1/ --url=downloads.hdwitness.com/clients/2.1/default/windows/x64/

int downloadFile(const QString& url, const QString& destFilePath)
{
    nx::utils::Url sourceUrl(url);

    std::string destFile = destFilePath.isEmpty()
        ? QFileInfo(sourceUrl.path()).fileName().toStdString()
        : destFilePath.toStdString();

    nx::network::http::HttpClient httpClient;
    httpClient.setUserName(sourceUrl.userName());
    httpClient.setUserPassword(sourceUrl.password());
    if (!httpClient.doGet(sourceUrl))
    {
        std::cerr << "Failed to get " << url.toStdString() << std::endl;
        return 1;
    }

    if ((httpClient.response()->statusLine.statusCode / 200 * 200) != nx::network::http::StatusCode::ok)
    {
        std::cerr << "Failed to get " << url.toStdString() << ". " << httpClient.response()->statusLine.reasonPhrase.constData() << std::endl;
        return 1;
    }

    std::ofstream f;
    f.open(destFile, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
    if (!f.is_open())
    {
        std::cerr << "Failed to open " << destFile << std::endl;
        return 1;
    }

    while (!httpClient.eof())
    {
        const nx::network::http::BufferType& buf = httpClient.fetchMessageBodyBuffer();
        f.write(buf.constData(), buf.size());
    }
    f.close();

    return 0;
}
