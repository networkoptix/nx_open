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
#include <utils/common/process.h>

#include "process_utils.h"


ApplauncherProcess::ApplauncherProcess(
    QSettings* const settings,
    InstallationManager* const installationManager,
    bool quitMode,
    const QString& mirrorListUrl )
:
    m_terminated( false ),
    m_installationManager( installationManager ),
    m_quitMode( quitMode ),
    m_mirrorListUrl( mirrorListUrl ),
    m_taskServer( this ),
    m_settings( settings ),
    m_bindTriesCount( 0 ),
    m_isLocalServerWasNotFound( false )
{
}

void ApplauncherProcess::pleaseStop()
{
    {
        std::lock_guard<std::mutex> lk( m_mutex );
        m_terminated = true;
        m_cond.notify_all();
    }

    std::for_each(
        m_killProcessTasks.begin(),
        m_killProcessTasks.end(),
        []( const std::pair<qint64, KillProcessTask>& val ){ TimerManager::instance()->joinAndDeleteTimer(val.first); } );
    m_killProcessTasks.clear();
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

        case applauncher::api::TaskType::installZip:
            *response = new applauncher::api::Response();
            installZip(
                std::static_pointer_cast<applauncher::api::InstallZipTask>( request ),
                static_cast<applauncher::api::Response*>(*response) );
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

        case applauncher::api::TaskType::getInstalledVersions:
            *response = new applauncher::api::GetInstalledVersionsResponse();
            getInstalledVersions(
                std::static_pointer_cast<applauncher::api::GetInstalledVersionsRequest>( request ),
                static_cast<applauncher::api::GetInstalledVersionsResponse*>(*response) );
            break;

        case applauncher::api::TaskType::cancelInstallation:
            *response = new applauncher::api::CancelInstallationResponse();
            cancelInstallation(
                std::static_pointer_cast<applauncher::api::CancelInstallationRequest>( request ),
                static_cast<applauncher::api::CancelInstallationResponse*>(*response) );
            break;

        case applauncher::api::TaskType::addProcessKillTimer:
            *response = new applauncher::api::AddProcessKillTimerResponse();
            addProcessKillTimer(
                std::static_pointer_cast<applauncher::api::AddProcessKillTimerRequest>( request ),
                static_cast<applauncher::api::AddProcessKillTimerResponse*>(*response) );
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
    QnSoftwareVersion versionToLaunch;
    QString appArgs;
    if( getVersionToLaunch( &versionToLaunch, &appArgs ) )
    {
        applauncher::api::Response response;
        for( int i = 0; i < 2; ++i )
        {
            if( startApplication(
                    std::make_shared<applauncher::api::StartApplicationTask>(versionToLaunch, appArgs),
                    &response ) )
                break;

            //failed to start, trying to restore version
            if( !blockingRestoreVersion( versionToLaunch ) )
                versionToLaunch = m_installationManager->latestVersion();
        }

        //if( response.result != applauncher::api::ResultType::ok )
    }

    std::unique_lock<std::mutex> lk( m_mutex );
    m_cond.wait( lk, [this](){ return m_terminated; } );

    //waiting for all running tasks to stop
    m_taskServer.pleaseStop();
    m_taskServer.wait();

    //interrupting running installation tasks. With taskserver down no one can modify m_activeInstallations
    for( auto installationIter: m_activeInstallations )
        installationIter.second->pleaseStop();

    for( auto installationIter: m_activeInstallations )
        installationIter.second->join();

    m_activeInstallations.clear();

    return 0;
}

QString ApplauncherProcess::devModeKey() const {
    return m_devModeKey;
}

void ApplauncherProcess::setDevModeKey(const QString &devModeKey) {
    m_devModeKey = devModeKey;
}

bool ApplauncherProcess::sendTaskToRunningLauncherInstance()
{
    NX_LOG( QString::fromLatin1("Entered AddingTaskToNamedPipe"), cl_logDEBUG1 );

    QByteArray serializedTask;
    if( !m_quitMode )
    {
        QnSoftwareVersion versionToLaunch;
        QString appArgs;
        if( !getVersionToLaunch( &versionToLaunch, &appArgs ) )
        {
            NX_LOG( QString::fromLatin1("Failed to find what to launch. Will not post any task to the named pipe"), cl_logDEBUG1 );
            return false;
        }

        applauncher::api::StartApplicationTask startTask(versionToLaunch, appArgs);
        startTask.autoRestore = true;
        serializedTask = startTask.serialize();
    }
    else
    {
        serializedTask = applauncher::api::QuitTask().serialize();
    }

    return addTaskToThePipe( serializedTask );
}

static const QLatin1String PREVIOUS_LAUNCHED_VERSION_PARAM_NAME( "previousLaunchedVersion" );

bool ApplauncherProcess::getVersionToLaunch( QnSoftwareVersion* const versionToLaunch, QString* const appArgs )
{
    if( m_settings->contains( PREVIOUS_LAUNCHED_VERSION_PARAM_NAME ) )
    {
        *versionToLaunch = QnSoftwareVersion(m_settings->value( PREVIOUS_LAUNCHED_VERSION_PARAM_NAME ).toString());
        *appArgs = m_settings->value( "previousUsedCmdParams" ).toString();
    }
    else if( m_installationManager->count() > 0 )
    {
        *versionToLaunch = m_installationManager->latestVersion();
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

    QnClientInstallationPtr installation = m_installationManager->installationForVersion(task->version);
    if (installation.isNull())
        installation = m_installationManager->installationForVersion(m_installationManager->latestVersion());

    if (installation.isNull()) {
        NX_LOG(QString::fromLatin1("Failed to find installed version %1 path").arg(task->version.toString()), cl_logDEBUG1);
        response->result = applauncher::api::ResultType::versionNotInstalled;
        return false;
    }

    task->version = installation->version();

    if( task->version != m_installationManager->latestVersion() ) {
        task->appArgs += QString::fromLatin1(" ") + m_settings->value( NON_RECENT_VERSION_ARGS_PARAM_NAME, NON_RECENT_VERSION_ARGS_DEFAULT_VALUE ).toString();

        if (installation->isNeedsVerification() && !installation->verify()) {
            NX_LOG(QString::fromLatin1("Verification failed for version %1 (path %2)").arg(installation->version().toString()).arg(installation->rootPath()), cl_logDEBUG1);
            response->result = applauncher::api::ResultType::ioError;

            if (task->autoRestore)
            {
                applauncher::api::StartInstallationResponse startInstallationResponse;
                startInstallation(
                    std::make_shared<applauncher::api::StartInstallationTask>( task->version, true ),
                    &startInstallationResponse );
            }

            return false;
        }
    }

    //TODO/IMPL start process asynchronously ?

    const QString binPath = installation->executableFilePath();

    QStringList environment = QProcess::systemEnvironment();
#ifdef Q_OS_LINUX
    QString variableValue = installation->libraryPath();
    if (!variableValue.isEmpty() && QFile::exists(variableValue)) {
        const QString variableName = "LD_LIBRARY_PATH";

        QRegExp varRegExp(QString("%1=(.+)").arg(variableName));

        auto it = environment.begin();
        for (; it != environment.end(); ++it) {
            if (varRegExp.exactMatch(*it)) {
                *it = QString("%1=%2:%3").arg(variableName, variableValue, varRegExp.cap(1));
                break;
            }
        }
        if (it == environment.end())
            environment.append(QString("%1=%2").arg(variableName, variableValue));
    }
#endif

    QStringList arguments = task->appArgs.split(QLatin1String(" "), QString::SkipEmptyParts);
    if (!m_devModeKey.isEmpty())
        arguments.append(QString::fromLatin1("--dev-mode-key=%1").arg(devModeKey()));

    NX_LOG( QString::fromLatin1("Launching version %1 (path %2)").arg(task->version.toString()).arg(binPath), cl_logDEBUG2 );
    if( ProcessUtils::startProcessDetached(
            binPath,
            arguments,
            QString(),
            environment) )
    {
        NX_LOG( QString::fromLatin1("Successfully launched version %1 (path %2)").arg(task->version.toString()).arg(binPath), cl_logDEBUG1 );
        m_settings->setValue( PREVIOUS_LAUNCHED_VERSION_PARAM_NAME, task->version.toString() );
        m_settings->sync();
        response->result = applauncher::api::ResultType::ok;
        return true;
    }
    else
    {
        //TODO/IMPL should mark version as not installed or corrupted?
        NX_LOG( QString::fromLatin1("Failed to launch version %1 (path %2)").arg(task->version.toString()).arg(binPath), cl_logDEBUG1 );
        response->result = applauncher::api::ResultType::ioError;

        if( task->autoRestore )
        {
            applauncher::api::StartInstallationResponse startInstallationResponse;
            startInstallation(
                std::make_shared<applauncher::api::StartInstallationTask>( task->version, true ),
                &startInstallationResponse );
        }

        return false;
    }
}

bool ApplauncherProcess::startInstallation(
    const std::shared_ptr<applauncher::api::StartInstallationTask>& task,
    applauncher::api::StartInstallationResponse* const response )
{
    //TODO/IMPL if installation of this version is already running, returning id of running installation

    //if already installed, running restore
    //if( m_installationManager->isVersionInstalled(task->version) )
    //{
    //    response->result = applauncher::api::ResultType::alreadyInstalled;
    //    return true;
    //}

    //detecting directory to download to 
    const QString& targetDir = m_installationManager->installationDirForVersion(task->version);
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
        targetDir,
        task->autoStart ) );
    if( !installationProcess->start( m_mirrorListUrl ) )
    {
        response->result = applauncher::api::ResultType::ioError;
        return true;
    }

    response->installationID = ++m_prevInstallationID;
    m_activeInstallations.insert( std::make_pair( response->installationID, installationProcess ) );

    connect( installationProcess.get(), &InstallationProcess::installationDone,
             this, &ApplauncherProcess::onInstallationDone, Qt::DirectConnection );

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
            default:
                break;
        }
        m_activeInstallations.erase( installationIter );
    }

    return true;
}

bool ApplauncherProcess::installZip(
    const std::shared_ptr<applauncher::api::InstallZipTask>& request,
    applauncher::api::Response* const response )
{
    bool ok = m_installationManager->installZip(request->version, request->zipFileName);
    response->result = ok ? applauncher::api::ResultType::ok : applauncher::api::ResultType::otherError;
    return ok;
}

bool ApplauncherProcess::isVersionInstalled(
    const std::shared_ptr<applauncher::api::IsVersionInstalledRequest>& request,
    applauncher::api::IsVersionInstalledResponse* const response )
{
    response->installed = m_installationManager->isVersionInstalled(request->version);
    return true;
}

bool ApplauncherProcess::getInstalledVersions(
    const std::shared_ptr<applauncher::api::GetInstalledVersionsRequest>& request,
    applauncher::api::GetInstalledVersionsResponse* const response )
{
    Q_UNUSED(request)
    response->versions = m_installationManager->installedVersions();
    return true;
}

bool ApplauncherProcess::cancelInstallation(
    const std::shared_ptr<applauncher::api::CancelInstallationRequest>& request,
    applauncher::api::CancelInstallationResponse* const response )
{
    auto it = m_activeInstallations.find(request->installationID);
    if (it == m_activeInstallations.end()) {
        response->result = applauncher::api::ResultType::otherError;
        return true;
    }

    it->second->cancel();

    response->result = applauncher::api::ResultType::ok;
    return true;
}

bool ApplauncherProcess::addProcessKillTimer(
    const std::shared_ptr<applauncher::api::AddProcessKillTimerRequest>& request,
    applauncher::api::AddProcessKillTimerResponse* const response )
{
    KillProcessTask task;
    task.processID = request->processID;
    {
        std::lock_guard<std::mutex> lk( m_mutex );
        if( m_terminated )
        {
            response->result = applauncher::api::ResultType::otherError;
            return true;
        }

        m_killProcessTasks[TimerManager::instance()->addTimer( this, request->timeoutMillis )] = task;
    }

    response->result = applauncher::api::ResultType::ok;
    return true;
}

void ApplauncherProcess::onTimer( const quint64& timerID )
{
    KillProcessTask task;
    {
        std::lock_guard<std::mutex> lk( m_mutex );
        if( m_terminated )
            return;

        auto it = m_killProcessTasks.find( timerID );
        if( it == m_killProcessTasks.end() )
            return;
        task = it->second;
        m_killProcessTasks.erase( it );
    }

    //stopping process if needed
    killProcessByPid( task.processID );
}

void ApplauncherProcess::onInstallationDone( InstallationProcess* installationProcess )
{
    if( installationProcess->getStatus() == applauncher::api::InstallationStatus::success )
    {
        m_installationManager->updateInstalledVersionsInformation();
        if( installationProcess->autoStartNeeded() )
        {
            applauncher::api::Response response;
            startApplication(
                std::make_shared<applauncher::api::StartApplicationTask>(installationProcess->getVersion(), QString()),
                &response );
        }
    }
}

static const int INSTALLATION_CHECK_TIMEOUT_MS = 1000;

bool ApplauncherProcess::blockingRestoreVersion( const QnSoftwareVersion& versionToLaunch )
{
    //trying to restore installed version
    applauncher::api::StartInstallationResponse startInstallationResponse;
    if( !startInstallation(
            std::make_shared<applauncher::api::StartInstallationTask>( versionToLaunch ),
            &startInstallationResponse ) )
    {
        return false;
    }

    applauncher::api::InstallationStatusResponse response;
    for( ;; )
    {
        getInstallationStatus(
            std::make_shared<applauncher::api::GetInstallationStatusRequest>( startInstallationResponse.installationID ),
            &response );

        if( response.status == applauncher::api::InstallationStatus::inProgress )
        {
            QThread::msleep( INSTALLATION_CHECK_TIMEOUT_MS );
            continue;
        }

        break;
    }

    return response.status == applauncher::api::InstallationStatus::success;
}
