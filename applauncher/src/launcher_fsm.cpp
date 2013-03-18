////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "launcher_fsm.h"

#include <QFinalState>

#include <utils/common/log.h>

#include "custom_transition.h"
#include "state/processing_application_task.h"
#include "task_pipe_name.h"
#include "version.h"


static const int maxBindTriesCount = 3;

LauncherFSM::LauncherFSM()
:
    m_fsmSharedData( m_installationManager ),
    m_taskServer( &m_taskQueue ),
    m_settings( QN_ORGANIZATION_NAME, QN_APPLICATION_NAME ),
    m_bindTriesCount( 0 ),
    m_previousAddTaskToPipeOperationResult( QLocalSocket::UnknownSocketError )
{
    initFSM();
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
    return m_previousAddTaskToPipeOperationResult == QLocalSocket::ServerNotFoundError
        || m_previousAddTaskToPipeOperationResult == QLocalSocket::ConnectionRefusedError
        || m_previousAddTaskToPipeOperationResult == QLocalSocket::PeerClosedError;
}

void LauncherFSM::initFSM()
{
    QState* bindingToLocalAddress = new QState( this );
    connect( bindingToLocalAddress, SIGNAL(entered()), this, SLOT(onBindingToLocalAddressEntered()) );
    setInitialState( bindingToLocalAddress );

    QState* addingTaskToNamedPipe = new QState( this );
    connect( addingTaskToNamedPipe, SIGNAL(entered()), this, SLOT(onAddingTaskToNamedPipeEntered()) );

    QState* waitingForTaskQueueBecomeNonEmpty = new QState( this );

    ProcessingApplicationTask* processingApplicationTask = new ProcessingApplicationTask(
        this,
        &m_settings,
        &m_installationManager,
        &m_fsmSharedData,
        &m_taskQueue );

    QFinalState* finalState = new QFinalState();


    //from bindingToLocalAddress
    {
        QAbstractTransition* tran = new QSignalTransition(
            this,
            SIGNAL(bindSucceeded()),
            bindingToLocalAddress );
        tran->setTargetState( waitingForTaskQueueBecomeNonEmpty );
        connect( tran, SIGNAL(triggered()), this, SLOT(addTaskToTheQueue()) );
        bindingToLocalAddress->addTransition( tran );
    }

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

    addingTaskToNamedPipe->addTransition( this, SIGNAL(failedToAddTaskToThePipe()), bindingToLocalAddress );


    //from waitingForTaskQueueBecomeNonEmpty
    waitingForTaskQueueBecomeNonEmpty->addTransition(
        &m_taskServer,
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
    if( m_taskServer.listen( taskPipeName ) )
        emit bindSucceeded();
    else
        emit bindFailed();
}

void LauncherFSM::onAddingTaskToNamedPipeEntered()
{
    NX_LOG( QString::fromLatin1("Entered AddingTaskToNamedPipe"), cl_logDEBUG1 );

    QString versionToLaunch;
    QString appArgs;
    if( !getVersionToLaunch( &versionToLaunch, &appArgs ) )
        ;   //TODO/IMPL

    //posting to the pipe 
    QLocalSocket sock;
    sock.connectToServer( taskPipeName );
    if( !sock.waitForConnected( -1 ) )
    {
        m_previousAddTaskToPipeOperationResult = sock.error();
        NX_LOG( QString::fromLatin1("Failed to connect to local server %1. %2").arg(taskPipeName).arg(sock.errorString()), cl_logDEBUG1 );
        emit failedToAddTaskToThePipe();
        return;
    }

    const QByteArray& serializedTask = StartApplicationTask(versionToLaunch, appArgs).serialize();
    if( sock.write( serializedTask.data(), serializedTask.size() ) != serializedTask.size() )
    {
        m_previousAddTaskToPipeOperationResult = sock.error();
        NX_LOG( QString::fromLatin1("Failed to send launch task to local server %1. %2").arg(taskPipeName).arg(sock.errorString()), cl_logDEBUG1 );
        emit failedToAddTaskToThePipe();
        return;
    }

    emit taskAddedToThePipe();
}

void LauncherFSM::addTaskToTheQueue()
{
    QString versionToLaunch;
    QString appArgs;
    if( !getVersionToLaunch( &versionToLaunch, &appArgs ) )
        ;   //TODO/IMPL
    m_taskQueue.push( StartApplicationTask(versionToLaunch, appArgs) );
}

bool LauncherFSM::getVersionToLaunch( QString* const versionToLaunch, QString* const appArgs )
{
    if( m_settings.contains( "previousLaunchedVersion" ) )
    {
        *versionToLaunch = m_settings.value( "previousLaunchedVersion" ).toString();
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
