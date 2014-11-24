/**********************************************************
* 20 sep 2013
* a.kolesnikov
***********************************************************/

#include "rdir_syncher.h"

#include <cassert>
#include <memory>

#include <utils/common/log.h>

#include "get_file_operation.h"


static const int MAX_SIMULTANEOUS_DOWNLOADS = 10;
static const int MAX_DOWNLOAD_RETRY_COUNT = 15;

RDirSyncher::EventReceiver::~EventReceiver()
{
}

void RDirSyncher::EventReceiver::overrallDownloadSizeKnown(
    const std::shared_ptr<RDirSyncher>& /*syncher*/,
    int64_t /*totalBytesToDownload*/ )
{
}

void RDirSyncher::EventReceiver::started( const std::shared_ptr<RDirSyncher>& /*syncher*/ )
{
}

void RDirSyncher::EventReceiver::fileStarted(
    const std::shared_ptr<RDirSyncher>& /*syncher*/,
    const QString& /*filePath*/ )
{
}

void RDirSyncher::EventReceiver::fileProgress(
    const std::shared_ptr<RDirSyncher>& /*syncher*/,
    const QString& /*filePath*/,
    int64_t /*remoteFileSize*/,
    int64_t /*bytesDownloaded*/ )
{
}

void RDirSyncher::EventReceiver::fileDone(
    const std::shared_ptr<RDirSyncher>& /*syncher*/,
    const QString& /*filePath*/ )
{
}

void RDirSyncher::EventReceiver::finished(
    const std::shared_ptr<RDirSyncher>& /*syncher*/,
    bool /*result*/ )
{
}

void RDirSyncher::EventReceiver::failed(
    const std::shared_ptr<RDirSyncher>& /*syncher*/,
    const QString& /*failedFilePath*/,
    const QString& /*errorText*/ )
{
}


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

void RDirSyncher::setFileProgressNotificationStep( int /*bytes*/ )
{
    //TODO/IMPL
}

bool RDirSyncher::startAsync()
{
    std::lock_guard<std::mutex> lk( m_mutex );

    NX_LOG( QString::fromLatin1("RDirSyncher. Starting synchronization from %1 to %2").arg(m_mirrors.front().toString()).arg(m_localDirPath), cl_logDEBUG1 );

    m_syncTasks.push_back( SynchronizationTask("/", detail::RSyncOperationType::listDirectory) );
    std::shared_ptr<detail::RDirSynchronizationOperation> operation;
    if( startNextOperation(&operation) != OperationStartResult::success )
        return false;
    m_state = sInProgress;
    return true;
}

RDirSyncher::State RDirSyncher::state() const
{
    return m_state;
}

void RDirSyncher::cancel()
{
    m_state = sInterrupted;
    // remove all non-running tasks and let the all other task finish
    std::remove_if(m_syncTasks.begin(), m_syncTasks.end(), [](const SynchronizationTask& task) { return !task.running; } );

    // cancel all active operations
    for (auto val: m_runningOperations)
        val.second->pleaseStop();
}
    
QString RDirSyncher::lastErrorText() const
{
    return m_lastErrorText;
}

void RDirSyncher::downloadProgress(
    const std::shared_ptr<detail::RDirSynchronizationOperation>& operation,
    int64_t remoteFileSize,
    int64_t bytesDownloaded )
{
    if( !m_eventReceiver )
        return;

    //TODO/IMPL report progress not very often

    //NX_LOG( QString::fromLatin1("File %1 download progress: %2 bytes of %3 have been downloaded").
    //    arg(operation->entryPath).arg(bytesDownloaded).arg(remoteFileSize), cl_logDEBUG2 );

    m_eventReceiver->fileProgress(
        shared_from_this(),
        operation->entryPath,
        remoteFileSize,
        bytesDownloaded );
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
    NX_LOG( QString::fromLatin1("RDirSyncher. Operation %1 %2").arg((int)operation->type).
        arg(operation->success() ? QLatin1String("completed successfully") : QLatin1String("failed")), cl_logDEBUG2 );

    std::list<RSyncEventTrigger*> eventsToTrigger;
    //creating auto guard which will trigger all events stored in eventsToTrigger
    auto SCOPED_GUARD_FUNC = [&eventsToTrigger](RDirSyncher* /*pThis*/)
    {
        for( RSyncEventTrigger* event: eventsToTrigger )
        {
            event->doAction();
            delete event;
        }
    };
    std::unique_ptr<RDirSyncher, decltype(SCOPED_GUARD_FUNC)> SCOPED_GUARD( this, SCOPED_GUARD_FUNC );

    //events are triggered with m_mutex unlocked, so that no dead-lock can occur if m_eventReceiver calls some method of this class 

    std::unique_lock<std::mutex> lk( m_mutex );

    std::map<int, std::shared_ptr<detail::RDirSynchronizationOperation>>::iterator opIter = m_runningOperations.find( operation->id );
    if( opIter == m_runningOperations.end() )
    {
        //TODO: unknown operation. What's the whooy?
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
                    eventsToTrigger.push_back( allocEventTrigger( std::bind(&EventReceiver::fileDone, m_eventReceiver, shared_from_this(), operation->entryPath) ) );
                break;

            case detail::RSyncOperationType::listDirectory:
            {
                std::shared_ptr<detail::ListDirectoryOperation> listDirOperation = std::static_pointer_cast<detail::ListDirectoryOperation>(operation);
                const std::list<detail::RDirEntry>& entries = listDirOperation->entries();
                //adding dir entries to download queue
                for( const detail::RDirEntry& entry: entries )
                    m_syncTasks.push_back( SynchronizationTask(entry) );
                if( listDirOperation->entryPath == "/" && listDirOperation->totalDirSize() > 0 )   //root directory
                    eventsToTrigger.push_back( allocEventTrigger(
                        std::bind(&EventReceiver::overrallDownloadSizeKnown, m_eventReceiver, shared_from_this(), listDirOperation->totalDirSize()) ) );
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
                eventsToTrigger.push_back( allocEventTrigger( std::bind(&EventReceiver::finished, m_eventReceiver, shared_from_this(), true) ) );
            return;
        }

        //starting next task(s)
        startOperations( eventsToTrigger );
        return;
    }

    //TODO: #ak refactor rest of this function in default branch

    if( operation->remoteSideFailure() )
    {
        if( completedTaskIter->retryCount < MAX_DOWNLOAD_RETRY_COUNT )
        {
            //repeating current operation
            ++completedTaskIter->retryCount;
            return startOperations( eventsToTrigger );
        }
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
            eventsToTrigger.push_back( allocEventTrigger( std::bind(&EventReceiver::failed, m_eventReceiver, shared_from_this(), m_failedEntryPath, m_lastErrorText) ) );
            eventsToTrigger.push_back( allocEventTrigger( std::bind(&EventReceiver::finished, m_eventReceiver, shared_from_this(), false) ) );
        }
        return;
    }

    //repeating operation with new mirror
    startOperations( eventsToTrigger );
}

void RDirSyncher::startOperations( std::list<RSyncEventTrigger*>& eventsToTrigger )
{
    //starting multiple operations
    while( m_runningOperations.size() < MAX_SIMULTANEOUS_DOWNLOADS )
    {
        if (m_state == sInterrupted)
            break;

        //starting next task
        std::shared_ptr<detail::RDirSynchronizationOperation> newOperation;
        switch( startNextOperation( &newOperation ) )
        {
            case OperationStartResult::success:
                if( newOperation->type == detail::RSyncOperationType::getFile && m_eventReceiver )
                    eventsToTrigger.push_back( allocEventTrigger( std::bind(&EventReceiver::fileStarted, m_eventReceiver, shared_from_this(), newOperation->entryPath) ) );
                break;

            case OperationStartResult::nothingToDo:
                return;

            case OperationStartResult::failure:
                //failure
                m_state = sFailed;
                //cancelling all active operations
                for( auto val: m_runningOperations )
                    val.second->pleaseStop();

                if( m_eventReceiver )
                {
                    eventsToTrigger.push_back( allocEventTrigger( std::bind(&EventReceiver::failed, m_eventReceiver, shared_from_this(), m_failedEntryPath, m_lastErrorText) ) );
                    eventsToTrigger.push_back( allocEventTrigger( std::bind(&EventReceiver::finished, m_eventReceiver, shared_from_this(), false) ) );
                }
                return;

            default:
                assert( false );
                break;
        }
    }
}

RDirSyncher::OperationStartResult RDirSyncher::startNextOperation( std::shared_ptr<detail::RDirSynchronizationOperation>* const newOperationRef )
{
    assert( !m_syncTasks.empty() );

    auto taskToStartIter = std::find_if( m_syncTasks.begin(), m_syncTasks.end(), [](const SynchronizationTask& task){ return !task.running; } );
    if( taskToStartIter == m_syncTasks.end() )
    {
        //all tasks are already started
        return OperationStartResult::nothingToDo;
    }

    SynchronizationTask& taskToStart = *taskToStartIter;
    //will remove task only after its successfull completion

    std::shared_ptr<detail::RDirSynchronizationOperation> opCtx;
    const int operationID = ++m_prevOperationID;
    switch( taskToStart.type )
    {
        case detail::RSyncOperationType::listDirectory: {
            //requesting file {taskToStart.entryPath}/contents.xml
            opCtx = std::make_shared<detail::ListDirectoryOperation>(
                operationID,
                m_currentMirror,
                taskToStart.entryPath,
                m_localDirPath,
                this );
            break;
        }

        case detail::RSyncOperationType::getFile: {
            //opCtx = std::make_shared<detail::GetFileOperation>(
            opCtx = std::shared_ptr<detail::GetFileOperation>( new detail::GetFileOperation(
                operationID,
                m_currentMirror,
                taskToStart.entryPath,
                m_localDirPath,
                taskToStart.hashTypeName,
                taskToStart.entrySize,
                this ) );
            break;
        }

        default:
            assert( false );
            return OperationStartResult::failure;
    }

    if( !opCtx->startAsync() )
    {
        //TODO/IMPL filling in m_failedEntryPath, m_lastErrorText
        return OperationStartResult::failure;
    }

    taskToStart.running = true;

    m_runningOperations.insert( std::make_pair( operationID, opCtx ) );
    *newOperationRef = opCtx;
    return OperationStartResult::success;
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
