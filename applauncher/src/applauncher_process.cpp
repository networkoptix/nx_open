/**********************************************************
* 20 sep 2013
* a.kolesnikov
***********************************************************/

#include "applauncher_process.h"

#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QtNetwork/QLocalSocket>

#include <api/ipc_pipe_names.h>
#include <utils/common/log.h>


ApplauncherProcess::ApplauncherProcess(
    QSettings* const settings,
    InstallationManager* const installationManager,
    bool quitMode )
:
    m_terminated( false ),
    m_installationManager( installationManager ),
    m_quitMode( quitMode ),
    m_taskServer( this ),
    m_settings( settings ),
    m_bindTriesCount( 0 ),
    m_isLocalServerWasNotFound( false )
{
}

void ApplauncherProcess::pleaseStop()
{
    std::lock_guard<std::mutex> lk( m_mutex );
    m_terminated = true;
    m_cond.notify_all();
}

void ApplauncherProcess::processRequest(
    const std::shared_ptr<applauncher::api::BaseTask>& request,
    applauncher::api::Response** const response )
{
    switch( request->type )
    {
        case applauncher::api::TaskType::quit:
            pleaseStop();
            break;

        case applauncher::api::TaskType::startApplication:
            *response = new applauncher::api::Response();
            startApplication(
                std::static_pointer_cast<applauncher::api::StartApplicationTask>( request ),
                *response );
            break;

        case applauncher::api::TaskType::install:
            *response = new applauncher::api::StartInstallationResponse();
            startInstallation(
                std::static_pointer_cast<applauncher::api::StartInstallationTask>( request ),
                static_cast<applauncher::api::StartInstallationResponse*>(*response) );
            break;

        case applauncher::api::TaskType::getInstallationStatus:
            *response = new applauncher::api::InstallationStatusResponse();
            getInstallationStatus(
                std::static_pointer_cast<applauncher::api::GetInstallationStatusRequest>( request ),
                static_cast<applauncher::api::InstallationStatusResponse*>(*response) );
            break;

        case applauncher::api::TaskType::isVersionInstalled:
            *response = new applauncher::api::IsVersionInstalledResponse();
            isVersionInstalled(
                std::static_pointer_cast<applauncher::api::IsVersionInstalledRequest>( request ),
                static_cast<applauncher::api::IsVersionInstalledResponse*>(*response) );
            break;

        case applauncher::api::TaskType::cancelInstallation:
            *response = new applauncher::api::CancelInstallationResponse();
            cancelInstallation(
                std::static_pointer_cast<applauncher::api::CancelInstallationRequest>( request ),
                static_cast<applauncher::api::CancelInstallationResponse*>(*response) );
            break;

        default:
            break;
    }
}

int ApplauncherProcess::run()
{
    if( !m_taskServer.listen( launcherPipeName ) )
    {
        //another instance already running?
        sendTaskToRunningLauncherInstance();
        return 0;
    }

    //we are the only running instance
    if( m_quitMode )
        return 0;

    //launching most recent version
    QString versionToLaunch;
    QString appArgs;
    if( getVersionToLaunch( &versionToLaunch, &appArgs ) )
    {
        applauncher::api::Response response;
        startApplication(
            std::make_shared<applauncher::api::StartApplicationTask>(versionToLaunch, appArgs),
            &response );
    }

    std::unique_lock<std::mutex> lk( m_mutex );
    m_cond.wait( lk, [this](){ return m_terminated; } );

    //waiting for all running tasks to stop
    m_taskServer.pleaseStop();
    m_taskServer.wait();

    //interrupting running installation tasks. With taskserver down noone can modify m_activeInstallations
    for( auto installationIter: m_activeInstallations )
        installationIter.second->pleaseStop();

    for( auto installationIter: m_activeInstallations )
        installationIter.second->wait();

    m_activeInstallations.clear();

    return 0;
}

bool ApplauncherProcess::sendTaskToRunningLauncherInstance()
{
    NX_LOG( QString::fromLatin1("Entered AddingTaskToNamedPipe"), cl_logDEBUG1 );

    QByteArray serializedTask;
    if( !m_quitMode )
    {
        QString versionToLaunch;
        QString appArgs;
        if( !getVersionToLaunch( &versionToLaunch, &appArgs ) )
        {
            NX_LOG( QString::fromLatin1("Failed to find what to launch. Will not post any task to the named pipe"), cl_logDEBUG1 );
            return false;
        }

        serializedTask = applauncher::api::StartApplicationTask(versionToLaunch, appArgs).serialize();
    }
    else
    {
        serializedTask = applauncher::api::QuitTask().serialize();
    }

    return addTaskToThePipe( serializedTask );
}

static const QLatin1String PREVIOUS_LAUNCHED_VERSION_PARAM_NAME( "previousLaunchedVersion" );

bool ApplauncherProcess::getVersionToLaunch( QString* const versionToLaunch, QString* const appArgs )
{
    if( m_settings->contains( PREVIOUS_LAUNCHED_VERSION_PARAM_NAME ) )
    {
        *versionToLaunch = m_settings->value( PREVIOUS_LAUNCHED_VERSION_PARAM_NAME ).toString();
        *appArgs = m_settings->value( "previousUsedCmdParams" ).toString();
    }
    else if( m_installationManager->count() > 0 )
    {
        *versionToLaunch = m_installationManager->getMostRecentVersion();
        //leaving default cmd params
    }
    else
    {
        NX_LOG( QString::fromLatin1("Failed to generate launch task. No client installed"), cl_logDEBUG1 );
        return false;
    }

    return true;
}

static const int MAX_MSG_LEN = 1024;

bool ApplauncherProcess::addTaskToThePipe( const QByteArray& serializedTask )
{
    //posting to the pipe 
#ifdef _WIN32
    NamedPipeSocket sock;
    SystemError::ErrorCode result = sock.connectToServerSync( launcherPipeName );
    if( result != SystemError::noError )
    {
        m_isLocalServerWasNotFound = result == SystemError::fileNotFound;
        NX_LOG( QString::fromLatin1("Failed to connect to local server %1. %2").arg(launcherPipeName).arg(SystemError::toString(result)), cl_logDEBUG1 );
        return false;
    }

    unsigned int bytesWritten = 0;
    result = sock.write( serializedTask.constData(), serializedTask.size(), &bytesWritten );
    if( (result != SystemError::noError) || (bytesWritten != serializedTask.size()) )
    {
        m_isLocalServerWasNotFound = result == SystemError::fileNotFound;
        NX_LOG( QString::fromLatin1("Failed to send launch task to local server %1. %2").arg(launcherPipeName).arg(SystemError::toString(result)), cl_logDEBUG1 );
        return false;
    }

    char buf[MAX_MSG_LEN];
    unsigned int bytesRead = 0;
    sock.read( buf, sizeof(buf), &bytesRead );  //ignoring return code

    return true;
#else
    QLocalSocket sock;
    sock.connectToServer( launcherPipeName );
    if( !sock.waitForConnected( -1 ) )
    {
        m_isLocalServerWasNotFound = sock.error() == QLocalSocket::ServerNotFoundError ||
                                     sock.error() == QLocalSocket::ConnectionRefusedError ||
                                     sock.error() == QLocalSocket::PeerClosedError;
        NX_LOG( QString::fromLatin1("Failed to connect to local server %1. %2").arg(launcherPipeName).arg(sock.errorString()), cl_logDEBUG1 );
        return false;
    }

    if( sock.write( serializedTask.data(), serializedTask.size() ) != serializedTask.size() )
    {
        m_isLocalServerWasNotFound = sock.error() == QLocalSocket::ServerNotFoundError ||
                                     sock.error() == QLocalSocket::ConnectionRefusedError ||
                                     sock.error() == QLocalSocket::PeerClosedError;
        NX_LOG( QString::fromLatin1("Failed to send launch task to local server %1. %2").arg(launcherPipeName).arg(sock.errorString()), cl_logDEBUG1 );
        return false;
    }

    sock.waitForReadyRead(-1);
    sock.readAll();

    return true;
#endif
}

static const QLatin1String NON_RECENT_VERSION_ARGS_PARAM_NAME( "nonRecentVersionArgs" );
static const QLatin1String NON_RECENT_VERSION_ARGS_DEFAULT_VALUE( "--updates-enabled=false" );

bool ApplauncherProcess::startApplication(
    const std::shared_ptr<applauncher::api::StartApplicationTask>& task,
    applauncher::api::Response* const response )
{
    NX_LOG( QString::fromLatin1("Entered LaunchingApplication"), cl_logDEBUG1 );

#ifdef AK_DEBUG
    std::const_pointer_cast<applauncher::api::StartApplicationTask>(task)->version = "debug";
#endif

    InstallationManager::AppData appData;
    for( ;; )
    {
        if( m_installationManager->getInstalledVersionData( task->version, &appData ) )
            break;

        if( m_installationManager->count() > 0 )
        {
            const QString& theMostRecentVersion = m_installationManager->getMostRecentVersion();
            if( task->version != theMostRecentVersion )
            {
                task->version = theMostRecentVersion;
                continue;
            }
        }
        NX_LOG( QString::fromLatin1("Failed to find installed version %1 path").arg(task->version), cl_logDEBUG1 );
        response->result = applauncher::api::ResultType::versionNotInstalled;
        return false;
    }

    if( task->version != m_installationManager->getMostRecentVersion() )
        task->appArgs += QString::fromLatin1(" ") + m_settings->value( NON_RECENT_VERSION_ARGS_PARAM_NAME, NON_RECENT_VERSION_ARGS_DEFAULT_VALUE ).toString();

    //TODO/IMPL start process asynchronously ?

    const QString binPath = appData.installationDirectory + "/" + APPLICATION_BIN_NAME;
    NX_LOG( QString::fromLatin1("Launching version %1 (path %2)").arg(task->version).arg(binPath), cl_logDEBUG2 );
    if( QProcess::startDetached(
            binPath,
            task->appArgs.split(QLatin1String(" "), QString::SkipEmptyParts) ) )
    {
        NX_LOG( QString::fromLatin1("Successfully launched version %1 (path %2)").arg(task->version).arg(binPath), cl_logDEBUG1 );
        m_settings->setValue( QLatin1String("previousLaunchedVersion"), task->version );
        response->result = applauncher::api::ResultType::ok;
        return true;
    }
    else
    {
        //TODO/IMPL should mark version as not installed or corrupted?
        NX_LOG( QString::fromLatin1("Failed to launch version %1 (path %2)").arg(task->version).arg(binPath), cl_logDEBUG1 );
        response->result = applauncher::api::ResultType::ioError;
        return false;
    }
}

bool ApplauncherProcess::startInstallation(
    const std::shared_ptr<applauncher::api::StartInstallationTask>& task,
    applauncher::api::StartInstallationResponse* const response )
{
    if( !InstallationManager::isValidVersionName(task->version) )
    {
        response->result = applauncher::api::ResultType::invalidVersionFormat;
        return true;
    }

    //if already installed, running restore
    //if( m_installationManager->isVersionInstalled(task->version) )
    //{
    //    response->result = applauncher::api::ResultType::alreadyInstalled;
    //    return true;
    //}

    //detecting directory to download to 
    const QString& targetDir = m_installationManager->getInstallDirForVersion(task->version);
    if( !QDir().mkpath(targetDir) )
    {
        response->result = applauncher::api::ResultType::ioError;
        return true;
    }
    
    std::shared_ptr<InstallationProcess> installationProcess( new InstallationProcess(
        QN_PRODUCT_NAME_SHORT,
        QN_CUSTOMIZATION_NAME,
        task->version,
        task->module,
        targetDir ) );
    if( !installationProcess->start( *m_settings ) )
    {
        response->result = applauncher::api::ResultType::ioError;
        return true;
    }

    response->installationID = ++m_prevInstallationID;
    m_activeInstallations.insert( std::make_pair( response->installationID, installationProcess ) );

    connect( installationProcess.get(), SIGNAL(installationSucceeded()), this, SLOT(onInstallationSucceeded()), Qt::DirectConnection );

    response->result = applauncher::api::ResultType::ok;
    return true;
}

bool ApplauncherProcess::getInstallationStatus(
    const std::shared_ptr<applauncher::api::GetInstallationStatusRequest>& request,
    applauncher::api::InstallationStatusResponse* const response )
{
    auto installationIter = m_activeInstallations.find( request->installationID );
    if( installationIter == m_activeInstallations.end() )
    {
        response->result = applauncher::api::ResultType::notFound;
        return true;
    }

    response->status = installationIter->second->getStatus();
    response->progress = installationIter->second->getProgress();

    if( response->status > applauncher::api::InstallationStatus::cancelInProgress )
    {
        switch( response->status )
        {
            case applauncher::api::InstallationStatus::success:
                NX_LOG( QString::fromLatin1("Installation finished successfully"), cl_logDEBUG2 );
                break;
            case applauncher::api::InstallationStatus::failed:
                NX_LOG( QString::fromLatin1("Installation has failed. %1").arg(installationIter->second->errorText()), cl_logDEBUG1 );
                break;
            case applauncher::api::InstallationStatus::cancelled:
                NX_LOG( QString::fromLatin1("Installation has been cancelled"), cl_logDEBUG2 );
                break;
        }
        m_activeInstallations.erase( installationIter );
    }

    return true;
}

bool ApplauncherProcess::isVersionInstalled(
    const std::shared_ptr<applauncher::api::IsVersionInstalledRequest>& request,
    applauncher::api::IsVersionInstalledResponse* const response )
{
    response->installed = m_installationManager->isVersionInstalled(request->version);
    return true;
}

bool ApplauncherProcess::cancelInstallation(
    const std::shared_ptr<applauncher::api::CancelInstallationRequest>& /*request*/,
    applauncher::api::CancelInstallationResponse* const response )
{
    //TODO/IMPL cancelling by id request->installationID
    response->result = applauncher::api::ResultType::otherError;
    return true;
}

void ApplauncherProcess::onInstallationSucceeded()
{
    m_installationManager->updateInstalledVersionsInformation();
}
