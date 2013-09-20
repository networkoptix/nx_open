/**********************************************************
* 20 sep 2013
* a.kolesnikov
***********************************************************/

#include "applauncher_process.h"

#include <QProcess>

#include <api/ipc_pipe_names.h>
#include <utils/common/log.h>

#include "version.h"


ApplauncherProcess::ApplauncherProcess( bool quitMode )
:
    m_quitMode( quitMode ),
    m_taskServer( &m_taskQueue ),
    m_settings( QN_ORGANIZATION_NAME, QN_APPLICATION_NAME ),
    m_bindTriesCount( 0 ),
    m_isLocalServerWasNotFound( false )
{
}

void ApplauncherProcess::pleaseStop()
{
    m_taskQueue.push( std::shared_ptr<applauncher::api::BaseTask>( new applauncher::api::QuitTask() ) );
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
        m_taskQueue.push( std::shared_ptr<applauncher::api::BaseTask>( new applauncher::api::StartApplicationTask(versionToLaunch, appArgs) ) );

    for( ;; )
    {
        std::shared_ptr<applauncher::api::BaseTask> task = m_taskQueue.front();
        m_taskQueue.pop();

        //processing task
        if( task->type == applauncher::api::TaskType::quit )
            break;
        else if( task->type == applauncher::api::TaskType::startApplication )
            startApplication( std::static_pointer_cast<applauncher::api::StartApplicationTask>( task ) );
    }

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
    if( m_settings.contains( PREVIOUS_LAUNCHED_VERSION_PARAM_NAME ) )
    {
        *versionToLaunch = m_settings.value( PREVIOUS_LAUNCHED_VERSION_PARAM_NAME ).toString();
        *appArgs = m_settings.value( "previousUsedCmdParams" ).toString();
    }
    else if( m_installationManager.count() > 0 )
    {
        *versionToLaunch = m_installationManager.getMostRecentVersion();
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
        const QString& errStr = sock.errorString();
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

#ifdef AK_DEBUG
static const QString APPLICATION_BIN_NAME( QString::fromLatin1("/%1").arg(QLatin1String("client.exe")) );
#else
static const QString APPLICATION_BIN_NAME( QString::fromLatin1("/%1").arg(QLatin1String(QN_CLIENT_EXECUTABLE_NAME)) );
#endif

static const QLatin1String NON_RECENT_VERSION_ARGS_PARAM_NAME( "nonRecentVersionArgs" );
static const QLatin1String NON_RECENT_VERSION_ARGS_DEFAULT_VALUE( "--updates-enabled=false" );

bool ApplauncherProcess::startApplication( const std::shared_ptr<applauncher::api::StartApplicationTask>& task )
{
    NX_LOG( QString::fromLatin1("Entered LaunchingApplication"), cl_logDEBUG1 );

    InstallationManager::AppData appData;
    for( ;; )
    {
        if( m_installationManager.getInstalledVersionData( task->version, &appData ) )
            break;

        if( m_installationManager.count() > 0 )
        {
            const QString& theMostRecentVersion = m_installationManager.getMostRecentVersion();
            if( task->version != theMostRecentVersion )
            {
                task->version = theMostRecentVersion;
                continue;
            }
        }
        NX_LOG( QString::fromLatin1("Failed to find installed version %1 path").arg(task->version), cl_logDEBUG1 );
        return false;
    }

    if( task->version != m_installationManager.getMostRecentVersion() )
        task->appArgs += QString::fromLatin1(" ") + m_settings.value( NON_RECENT_VERSION_ARGS_PARAM_NAME, NON_RECENT_VERSION_ARGS_DEFAULT_VALUE ).toString();

    const QString binPath = appData.installationDirectory + "/" + APPLICATION_BIN_NAME;
    NX_LOG( QString::fromLatin1("Launching version %1 (path %2)").arg(task->version).arg(binPath), cl_logDEBUG2 );
    if( QProcess::startDetached(
            binPath,
            task->appArgs.split(QLatin1String(" "), QString::SkipEmptyParts) ) )
    {
        NX_LOG( QString::fromLatin1("Successfully launched version %1 (path %2)").arg(task->version).arg(binPath), cl_logDEBUG1 );
        m_settings.setValue( QLatin1String("previousLaunchedVersion"), task->version );
        return true;
    }
    else
    {
        NX_LOG( QString::fromLatin1("Failed to launch version %1 (path %2)").arg(task->version).arg(binPath), cl_logDEBUG1 );
        return false;
    }
}
