////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "launcher_fsm.h"

#include <QFinalState>
#include <QLocalSocket>

#include <api/ipc_pipe_names.h>
#include <utils/common/log.h>

#include "custom_transition.h"
#include "state/processing_application_task.h"
#include "version.h"


static const int maxBindTriesCount = 3;

LauncherFSM::LauncherFSM( bool quitMode )
:
    m_quitMode( quitMode ),
    m_fsmSharedData( m_installationManager ),
    m_taskServer( &m_taskQueue ),
    m_settings( QN_ORGANIZATION_NAME, QN_APPLICATION_NAME ),
    m_bindTriesCount( 0 ),
    m_isLocalServerWasNotFound( false ),
    m_taskQueueWatcher( &m_taskQueue )
{
    initFSM();
    m_taskQueueWatcher.start();
}

bool LauncherFSM::isTaskQueueEmpty() const
{
    return m_taskQueue.empty();
}

int LauncherFSM::bindTriesCount() const
{
    return m_bindTriesCount;
}

bool LauncherFSM::isLocalServerWasNotFound() const
{
    return m_isLocalServerWasNotFound;
}

bool LauncherFSM::quitMode() const
{
    return m_quitMode;
}

void LauncherFSM::initFSM()
{
    QState* bindingToLocalAddress = new QState( this );
    connect( bindingToLocalAddress, SIGNAL(entered()), this, SLOT(onBindingToLocalAddressEntered()) );
    setInitialState( bindingToLocalAddress );

    QState* addingTaskToNamedPipe = new QState( this );
    connect( addingTaskToNamedPipe, SIGNAL(entered()), this, SLOT(onAddingTaskToNamedPipeEntered()) );

    QState* waitingForTaskQueueBecomeNonEmpty = new QState( this );
    connect( waitingForTaskQueueBecomeNonEmpty, SIGNAL(entered()), &m_taskQueueWatcher, SLOT(startMonitoring()) );
    connect( waitingForTaskQueueBecomeNonEmpty, SIGNAL(exited()), &m_taskQueueWatcher, SLOT(stopMonitoring()) );

    ProcessingApplicationTask* processingApplicationTask = new ProcessingApplicationTask(
        this,
        &m_settings,
        &m_installationManager,
        &m_fsmSharedData,
        &m_taskQueue );

    QFinalState* finalState = new QFinalState();


    //from bindingToLocalAddress
    {
        ConditionalSignalTransition* tran = new ConditionalSignalTransition(
            this,
            SIGNAL(bindSucceeded()),
            bindingToLocalAddress,
            waitingForTaskQueueBecomeNonEmpty );
        tran->addCondition( new ObjectPropertyEqualConditionHelper<int>::CondType(this, "quitMode", false) );
        connect( tran, SIGNAL(triggered()), this, SLOT(addTaskToTheQueue()) );
        bindingToLocalAddress->addTransition( tran );
    }

    bindingToLocalAddress->addTransition(
        new ConditionalSignalTransition(
            this,
            SIGNAL(bindSucceeded()),
            bindingToLocalAddress,
            finalState,
            new ObjectPropertyEqualConditionHelper<int>::CondType(this, "quitMode", true) ) );

    bindingToLocalAddress->addTransition( this, SIGNAL(bindFailed()), addingTaskToNamedPipe );


    //from addingTaskToNamedPipe
    addingTaskToNamedPipe->addTransition(
        this,
        SIGNAL(taskAddedToThePipe()),
        finalState );

    {
        ConditionalSignalTransition* tran = new ConditionalSignalTransition(
            this,
            SIGNAL(failedToAddTaskToThePipe()),
            addingTaskToNamedPipe,
            bindingToLocalAddress );
        tran->addCondition( new ObjectPropertyLessConditionHelper<int>::CondType(this, "bindTriesCount", maxBindTriesCount) );
        tran->addCondition( new ObjectPropertyEqualConditionHelper<bool>::CondType(this, "isLocalServerWasNotFound", true) );
        addingTaskToNamedPipe->addTransition( tran );
    }

    addingTaskToNamedPipe->addTransition( this, SIGNAL(failedToAddTaskToThePipe()), finalState );


    //from waitingForTaskQueueBecomeNonEmpty
    waitingForTaskQueueBecomeNonEmpty->addTransition(
        &m_taskQueueWatcher,
        SIGNAL(taskReceived()),
        processingApplicationTask );


    //from processingApplicationTask
    processingApplicationTask->addTransition(
        new ConditionalSignalTransition(
            processingApplicationTask,
            SIGNAL(finished()),
            processingApplicationTask,
            waitingForTaskQueueBecomeNonEmpty,
            new ObjectPropertyEqualConditionHelper<bool>::CondType(this, "isTaskQueueEmpty", true) ) );

    processingApplicationTask->addTransition(
        new ConditionalSignalTransition(
            processingApplicationTask,
            SIGNAL(finished()),
            processingApplicationTask,
            processingApplicationTask,
            new ObjectPropertyEqualConditionHelper<bool>::CondType(this, "isTaskQueueEmpty", false) ) );
}

void LauncherFSM::onBindingToLocalAddressEntered()
{
    NX_LOG( QString::fromLatin1("Entered BindingToLocalAddress"), cl_logDEBUG1 );

    ++m_bindTriesCount;

    //binding to the pipe address
    if( m_taskServer.listen( launcherPipeName ) )
        emit bindSucceeded();
    else
        emit bindFailed();
}

static const QLatin1String PREVIOUS_LAUNCHED_VERSION_PARAM_NAME( "previousLaunchedVersion" );

void LauncherFSM::onAddingTaskToNamedPipeEntered()
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
            emit failedToAddTaskToThePipe();
            return;
        }

        serializedTask = applauncher::api::StartApplicationTask(versionToLaunch, appArgs).serialize();
    }
    else
    {
        serializedTask = applauncher::api::QuitTask().serialize();
    }

    if( addTaskToThePipe( serializedTask ) )
        emit taskAddedToThePipe();
    else
        emit failedToAddTaskToThePipe();
}

bool LauncherFSM::addTaskToTheQueue()
{
    QString versionToLaunch;
    QString appArgs;
    if( !getVersionToLaunch( &versionToLaunch, &appArgs ) )
        return false;   //TODO/IMPL no single version installed
    m_taskQueue.push( std::shared_ptr<applauncher::api::BaseTask>( new applauncher::api::StartApplicationTask(versionToLaunch, appArgs) ) );
    return true;
}

bool LauncherFSM::getVersionToLaunch( QString* const versionToLaunch, QString* const appArgs )
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

bool LauncherFSM::addTaskToThePipe( const QByteArray& serializedTask )
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
