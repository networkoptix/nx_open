////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include <iomanip>
#include <iostream>

#include <qtsinglecoreapplication.h>
#include <QDir>
#include <QThread>

#include <utils/common/command_line_parser.h>
#include <utils/common/log.h>
#include <utils/common/app_info.h>

#include "applauncher_process.h"
#include "installation_process.h"
#include "rdir_syncher.h"
#include "process_utils.h"

#ifdef _WIN32
#include <windows.h>

static ApplauncherProcess* applauncherProcessInstance = 0;

static BOOL WINAPI stopServer_WIN(DWORD /*dwCtrlType*/)
{
    if( applauncherProcessInstance )
        applauncherProcessInstance->pleaseStop();
    return TRUE;
}
#endif

static QString SERVICE_NAME = QString::fromLatin1("%1%2").arg(QnAppInfo::customizationName()).arg(QLatin1String("AppLauncher"));

static void printHelp()
{
    std::cout<<
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
        "  Installation mode (installs requested version)\n"
        "    --install Run in installation mode (install version specified by following parameters)\n"
        "    --version= Mandatory in installation mode\n"
        "    --product-name= Optional. By default, \""<<QnAppInfo::productNameShort().toUtf8().data()<<"\"\n"
        "    --customization= Optional. By default, \""<<QnAppInfo::customizationName().toUtf8().data()<<"\"\n"
        "    --module= Optional. By default, \"client\"\n"
        "    --install-path= Optional. By default, " << InstallationManager::defaultDirectoryForInstallations().toStdString() << "/{version}\n"
        "\n"
        "  --help. This help message\n"
        "\n";
}

int syncDir( const QString& localDir, const QString& remoteUrl );
int doInstallation(
    const InstallationManager& installationManager,
    const QString& mirrorListUrl,
    const QString& productName,
    const QString& customization,
    const QnSoftwareVersion &version,
    const QString& module,
    const QString& installationPath );

int downloadFile( const QString& url, const QString& destFilePath );

int main( int argc, char* argv[] )
{
    //return testHttpClient();

    QnLongRunnablePool runnablePool;

    QString logLevel = "WARN";
    QString installationsDir = InstallationManager::defaultDirectoryForInstallations();
    if (!QDir(installationsDir).exists())
        QDir().mkpath(installationsDir);
    QString logFilePath = installationsDir + "/applauncher";
    QString mirrorListUrl;
    bool quitMode = false;
    bool displayHelp = false;

    bool syncMode = false;
    QString localDir;
    QString remoteUrl;
    bool installMode = false;
    QString versionToInstall;
    QString productNameToInstall( QnAppInfo::productNameShort() );
    QString customizationToInstall( QnAppInfo::customizationName() );
    QString moduleToInstall( QString::fromLatin1("client") );
    QString installationPath;
    QString devModeKey;
    
    QString fileToDownload;
    QString destFilePath;

    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter( &logLevel, "--log-level", NULL, QString() );
    commandLineParser.addParameter( &logFilePath, "--log-file", NULL, QString() );
    commandLineParser.addParameter( &mirrorListUrl, "--mirror-list-url", NULL, QString() );
    commandLineParser.addParameter( &quitMode, "quit", NULL, QString(), QVariant(true) );
    commandLineParser.addParameter( &displayHelp, "--help", NULL, QString(), QVariant(true) );
    commandLineParser.addParameter( &syncMode, "--rsync", NULL, QString(), QVariant(true) );
    commandLineParser.addParameter( &localDir, "--dir", NULL, QString(), QString() );
    commandLineParser.addParameter( &remoteUrl, "--url", NULL, QString(), QString() );
    commandLineParser.addParameter( &installMode, "--install", NULL, QString(), QVariant(true) );
    commandLineParser.addParameter( &versionToInstall, "--version", NULL, QString(), QString() );
    commandLineParser.addParameter( &productNameToInstall, "--product-name", NULL, QString(), QString() );
    commandLineParser.addParameter( &customizationToInstall, "--customization", NULL, QString(), QString() );
    commandLineParser.addParameter( &moduleToInstall, "--module", NULL, QString(), QString() );
    commandLineParser.addParameter( &installationPath, "--install-path", NULL, QString(), QString() );
    commandLineParser.addParameter( &devModeKey, "--dev-mode-key", NULL, QString(), QString() );
    commandLineParser.addParameter( &fileToDownload, "--get", "-g", QString(), QString() );
    commandLineParser.addParameter( &destFilePath, "--out-file", "-o", QString(), QString() );
    commandLineParser.parse( argc, argv, stderr );

    QtSingleCoreApplication app( SERVICE_NAME, argc, argv );
    QDir::setCurrent( QCoreApplication::applicationDirPath() );

    QSettings globalSettings( QSettings::SystemScope, QnAppInfo::organizationName(), QnAppInfo::applicationName() );
    QSettings userSettings( QSettings::UserScope, QnAppInfo::organizationName(), QnAppInfo::applicationName() );

    if( mirrorListUrl.isEmpty() )
        mirrorListUrl = globalSettings.value( "mirrorListUrl", QnAppInfo::mirrorListUrl() ).toString();

    if (mirrorListUrl.isEmpty())
        NX_LOG( "MirrorListUrl is empty", cl_logWARNING );

    if( displayHelp )
    {
        printHelp();
        return 0;
    }

    //initialize logging based on args
    if( !logFilePath.isEmpty() && !logLevel.isEmpty() )
    {
        cl_log.create( logFilePath, 1024*1024*10, 5, cl_logWARNING );
        QnLog::initLog( logLevel );
    }

    NX_LOG( QnAppInfo::applicationName() + " started", cl_logALWAYS );
    NX_LOG( "Software version: " + QnAppInfo::applicationVersion(), cl_logALWAYS );
    NX_LOG( "Software revision: " + QnAppInfo::applicationRevision(), cl_logALWAYS );

    InstallationManager installationManager;

    if( !fileToDownload.isEmpty() )
        return downloadFile( fileToDownload, destFilePath );
    if( syncMode )
        return syncDir( localDir, remoteUrl );
    if( installMode )
        return doInstallation(
            installationManager,
            mirrorListUrl,
            productNameToInstall,
            customizationToInstall,
            QnSoftwareVersion(versionToInstall),
            moduleToInstall,
            installationPath );

    ProcessUtils::initialize();

    ApplauncherProcess applauncherProcess(
        &userSettings,
        &installationManager,
        quitMode,
        mirrorListUrl );

    applauncherProcess.setDevModeKey(devModeKey);

#ifdef _WIN32
    applauncherProcessInstance = &applauncherProcess;
    SetConsoleCtrlHandler(stopServer_WIN, true);
#endif

    int status = applauncherProcess.run();

#ifdef _WIN32
    if( quitMode )
    {
        // Wait for app to finish + 100ms just in case (in may be still running after unlocking QSingleApplication lock file).
        while (app.isRunning()) {
            Sleep(100);
        } 
        Sleep(100);
    }
#endif

    return status;
}

//--rsync --url=http://downloads.hdwitness.com/test/x64/ --dir=c:/tmp/appl/
//--install --customization=digitalwatchdog --version=1.5

int syncDir( const QString& localDir, const QString& remoteUrl )
{
    if( localDir.isEmpty() )
    {
        std::cerr<<"Error: No target directory specified"<<std::endl;
        return 1;
    }
    if( remoteUrl.isEmpty() )
    {
        std::cerr<<"Error: No remote url specified"<<std::endl;
        return 1;
    }

    RDirSyncher::EventReceiver dummyEventReceiver;
    auto syncher = std::make_shared<RDirSyncher>( QUrl(remoteUrl), localDir, &dummyEventReceiver );
    if( !syncher->startAsync() )
    {
        std::cerr<<"Error: Failed to start synchronizaion"<<std::endl;
        return 2;
    }

    while( syncher->state() == RDirSyncher::sInProgress )
        QThread::msleep( 1000 );

    switch( syncher->state() )
    {
        case RDirSyncher::sSucceeded:
            std::cout<<"Synchronization finished"<<std::endl;
            return 0;

        case RDirSyncher::sInterrupted:
            std::cerr<<"Synchronization has been interrupted"<<std::endl;
            return 3;

        case RDirSyncher::sFailed:
            std::cerr<<"Synchronization has failed: "<<syncher->lastErrorText().toStdString()<<std::endl;
            return 3;

        default:
            std::cerr<<"Synchronization ended in unknown state "<<static_cast<int>(syncher->state())<<std::endl;
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
    const QString& installationPath )
{
    if( version.isNull() )
    {
        std::cerr<<"FAILURE. Missing required parameter \"version\""<<std::endl;
        return 1;
    }

    QString effectiveInstallationPath = installationPath;
    if( effectiveInstallationPath.isEmpty() )
        effectiveInstallationPath = installationManager.installationDirForVersion(version);

    InstallationProcess installationProcess(
        productName,
        customization,
        version,
        module,
        effectiveInstallationPath,
        false );
    if( !installationProcess.start( mirrorListUrl ) )
    {
        std::cerr<<"FAILURE. Cannot start installation. "<<installationProcess.errorText().toStdString()<<std::endl;
        return 1;
    }

    while( installationProcess.getStatus() <= applauncher::api::InstallationStatus::inProgress )
    {
        static const unsigned int INSTALLATION_STATUS_CHECK_TIMEOUT_MS = 1000;
        QThread::msleep( INSTALLATION_STATUS_CHECK_TIMEOUT_MS );
        std::cout<<"Current progress: "<<std::fixed<<std::setprecision(2)<<installationProcess.getProgress()<<"%"<<std::endl;
    }

    switch( installationProcess.getStatus() )
    {
        case applauncher::api::InstallationStatus::success:
            std::cout<<"SUCCESS"<<std::endl;
            break;

        case applauncher::api::InstallationStatus::failed:
            std::cerr<<"FAILURE. "<<installationProcess.errorText().toStdString()<<std::endl;
            break;

        default:
            assert( false );
    }

    return 0;
}


#include <fstream>
#include <string.h>

#include <utils/network/http/httpclient.h>

//--rsync --dir=c:/temp/1 --url=http://enk.me/clients/2.1/default/windows/x64/

int downloadFile( const QString& url, const QString& destFilePath )
{
    QUrl sourceUrl( url );

    std::string destFile = destFilePath.isEmpty()
        ? QFileInfo( sourceUrl.path() ).fileName().toStdString()
        : destFilePath.toStdString();

    nx_http::HttpClient httpClient;
    httpClient.setUserName( sourceUrl.userName() );
    httpClient.setUserPassword( sourceUrl.password() );
    if( !httpClient.doGet( sourceUrl ) )
    {
        std::cerr << "Failed to get " << url.toStdString() << std::endl;
        return 1;
    }

    if( (httpClient.response()->statusLine.statusCode / 200 * 200) != nx_http::StatusCode::ok )
    {
        std::cerr << "Failed to get " << url.toStdString() << ". " << httpClient.response()->statusLine.reasonPhrase.constData() << std::endl;
        return 1;
    }

    std::ofstream f;
    f.open( destFile, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary );
    if( !f.is_open() )
    {
        std::cerr << "Failed to open " << destFile << std::endl;
        return 1;
    }

    while( !httpClient.eof() )
    {
        const nx_http::BufferType& buf = httpClient.fetchMessageBodyBuffer();
        f.write( buf.constData(), buf.size() );
    }
    f.close();

    return 0;
}
