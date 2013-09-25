/**********************************************************
* 20 sep 2013
* a.kolesnikov
***********************************************************/

#include "rdir_syncher.h"

#include <cassert>
#include <memory>

#include "get_file_operation.h"


RDirSyncher::RDirSyncher(
    const std::forward_list<QUrl>& mirrors,
    const QString& localDirPath,
    RDirSyncher::EventReceiver* const eventReceiver )
:
    m_mirrors( mirrors ),
    m_localDirPath( localDirPath ),
    m_eventReceiver( eventReceiver ),
    m_state( sInit )
{
    assert( !m_mirrors.empty() );
    m_currentMirror = m_mirrors.front();
}

RDirSyncher::RDirSyncher(
    const QUrl& baseUrl,
    const QString& localDirPath,
    RDirSyncher::EventReceiver* const eventReceiver )
:
    m_localDirPath( localDirPath ),
    m_eventReceiver( eventReceiver ),
    m_state( sInit )
{
    m_mirrors.push_front( baseUrl );
    m_currentMirror = baseUrl;
}

RDirSyncher::~RDirSyncher()
{
}

void RDirSyncher::pleaseStop()
{
    //TODO/IMPL
}

void RDirSyncher::setFileProgressNotificationStep( int bytes )
{
    //TODO/IMPL
}

bool RDirSyncher::startAsync()
{
    std::lock_guard<std::mutex> lk( m_mutex );

    m_syncTasks.push_back( SynchronizationTask("/", detail::RSyncOperationType::listDirectory) );
    if( startNextOperation() == nullptr )
        return false;
    m_state = sInProgress;
    return true;
}

RDirSyncher::State RDirSyncher::state() const
{
    return m_state;
}
    
QString RDirSyncher::lastErrorText() const
{
    return m_lastErrorText;
}

class RSyncEventTrigger
{
public:
    virtual ~RSyncEventTrigger() {}

    virtual void doAction() = 0;
};

template<typename _Func>
class RSyncEventTriggerImpl
:
    public RSyncEventTrigger
{
public:
    RSyncEventTriggerImpl( const _Func& f ) : m_f( f ) {}

    virtual void doAction() override { m_f(); }

private:
    _Func m_f;
};

template<typename _Func>
RSyncEventTriggerImpl<_Func>* allocEventTrigger( const _Func& f )
{
    return new RSyncEventTriggerImpl<_Func>(f);
};

//TODO/IMPL: #ak I bet there is a better way for that in c++11

void RDirSyncher::operationDone( const std::shared_ptr<detail::RDirSynchronizationOperation>& operation )
{
    std::list<RSyncEventTrigger*> eventToTrigger;
    //creating auto guard which will trigger all events stored in eventToTrigger
    auto triggerEventFunc = [](std::list<RSyncEventTrigger*>* events)
    {
        for( RSyncEventTrigger* event: *events ) { event->doAction(); delete event; }
    };
    std::unique_ptr<std::list<RSyncEventTrigger*>, decltype(triggerEventFunc)> eventToTriggerGuard( &eventToTrigger, triggerEventFunc );

    //events are triggered with m_mutex unlocked, so that no dead-lock can occur if m_eventReceiver calls some method of this class 

    std::unique_lock<std::mutex> lk( m_mutex );

    std::map<int, std::shared_ptr<detail::RDirSynchronizationOperation>>::iterator opIter = m_runningOperations.find( operation->id );
    if( opIter == m_runningOperations.end() )
    {
        //TODO: unknown operation. What's the huy?
        assert( false );
        return;
    }

    m_runningOperations.erase( opIter );

    if( m_state > sInProgress )
        return; //already stopped, not interesting

    auto completedTaskIter = std::find_if(
        m_syncTasks.begin(), m_syncTasks.end(), [operation](const SynchronizationTask& task){ return task.entryPath == operation->entryPath; } );
    completedTaskIter->running = false;

    if( operation->success() )
    {
        //marking task as done
        m_syncTasks.erase( completedTaskIter );
        switch( operation->type )
        {
            case detail::RSyncOperationType::getFile:
                if( m_eventReceiver )
                    eventToTrigger.push_back( allocEventTrigger( std::bind(&EventReceiver::fileDone, m_eventReceiver, this, operation->entryPath) ) );
                break;

            case detail::RSyncOperationType::listDirectory:
            {
                //adding dir entries to download queue
                std::shared_ptr<detail::ListDirectoryOperation> listDirOperation = std::static_pointer_cast<detail::ListDirectoryOperation>(operation);
                const std::list<detail::RDirEntry>& entries = listDirOperation->entries();
                std::transform(
                    entries.begin(),
                    entries.end(),
                    std::back_inserter(m_syncTasks),
                    []( const detail::RDirEntry& _entry ){ return SynchronizationTask(_entry); } );
                break;
            }

            default:
                assert( false );
        }

        if( m_syncTasks.empty() )
        {
            //if no task to do - success
            assert( m_runningOperations.empty() );
            m_state = sSucceeded;
            if( m_eventReceiver )
                eventToTrigger.push_back( allocEventTrigger( std::bind(&EventReceiver::finished, m_eventReceiver, this, true) ) );
            return;
        }

        //starting next task(s)
        startOperations( eventToTrigger );
        return;
    }

    //preventing multiple mirror switch (e.g., if multiple simultaneous operations fail on same mirror, only one mirror change MUST occur)
    if( !operation->remoteSideFailure() ||  //if problem on local side, assuming synchronization as failed
        !useNextMirror(operation->baseUrl) )                  //no mirrors left to try: failure
    {
        m_state = sFailed;
        //cancelling all active operations
        for( auto val: m_runningOperations )
            val.second->pleaseStop();

        //saving last error text
        m_lastErrorText = operation->errorText();
        m_failedEntryPath = operation->entryPath;

        if( m_eventReceiver )
        {
            eventToTrigger.push_back( allocEventTrigger( std::bind(&EventReceiver::failed, m_eventReceiver, this, m_failedEntryPath, m_lastErrorText) ) );
            eventToTrigger.push_back( allocEventTrigger( std::bind(&EventReceiver::finished, m_eventReceiver, this, false) ) );
        }
        return;
    }

    //repeating operation with new mirror
    startOperations( eventToTrigger );
}

void RDirSyncher::startOperations( std::list<RSyncEventTrigger*>& eventToTrigger )
{
    //TODO/IMPL starting multiple operations

    //starting next task
    std::shared_ptr<detail::RDirSynchronizationOperation> newOperation = startNextOperation();
    if( !newOperation )
    {
        //failure
        m_state = sFailed;
        //cancelling all active operations
        for( auto val: m_runningOperations )
            val.second->pleaseStop();

        if( m_eventReceiver )
        {
            eventToTrigger.push_back( allocEventTrigger( std::bind(&EventReceiver::failed, m_eventReceiver, this, m_failedEntryPath, m_lastErrorText) ) );
            eventToTrigger.push_back( allocEventTrigger( std::bind(&EventReceiver::finished, m_eventReceiver, this, false) ) );
        }
        return;
    }

    if( newOperation->type == detail::RSyncOperationType::getFile && m_eventReceiver )
        eventToTrigger.push_back( allocEventTrigger( std::bind(&EventReceiver::fileStarted, m_eventReceiver, this, newOperation->entryPath) ) );
}

std::shared_ptr<detail::RDirSynchronizationOperation> RDirSyncher::startNextOperation()
{
    assert( !m_syncTasks.empty() );

    SynchronizationTask& taskToStart = *std::find_if( m_syncTasks.begin(), m_syncTasks.end(), [](const SynchronizationTask& task){ return !task.running; } );
    //will remove task only after its successfull completion

    std::shared_ptr<detail::RDirSynchronizationOperation> opCtx;
    const int operationID = ++m_prevOperationID;
    switch( taskToStart.type )
    {
        case detail::RSyncOperationType::listDirectory:
            //requesting file {taskToStart.entryPath}/contents.xml
            opCtx.reset( new detail::ListDirectoryOperation(
                operationID,
                m_currentMirror,
                taskToStart.entryPath,
                this ) );
            break;

        case detail::RSyncOperationType::getFile:
            opCtx.reset( new detail::GetFileOperation(
                operationID,
                m_currentMirror,
                m_localDirPath,
                taskToStart.entryPath,
                taskToStart.hashTypeName,
                this ) );
            break;

        default:
            assert( false );
            return nullptr;
    }

    if( !opCtx->startAsync() )
    {
        //TODO/IMPL filling in m_failedEntryPath, m_lastErrorText
        return nullptr;
    }

    taskToStart.running = true;

    m_runningOperations.insert( std::make_pair( operationID, opCtx ) );
    return opCtx;
}

bool RDirSyncher::useNextMirror( const QUrl& hintMirror )
{
    QUrl prevMirror;
    for( const QUrl& mirror: m_mirrors )
    {
        if( prevMirror == hintMirror )
        {
            m_currentMirror = mirror;
            return true;
        }
    }

    return false;
}
