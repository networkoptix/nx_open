/**********************************************************
* 13 feb 2015
* akolesnikov@networkoptix.com
***********************************************************/

#if defined(USE_OWN_MUTEX)

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__linux__)
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "mutex_lock_analyzer.h"

#include <iostream>

#include <QtGlobal>
#include <QtCore/QMutexLocker>

#include "mutex.h"
#include "mutex_impl.h"
#include "thread_util.h"
#include <nx/utils/log/log.h>

#if defined(ANALYZE_MUTEX_LOCKS_FOR_DEADLOCK)
    static bool kDoAnalyseForDeadlock = true;
#else
    static bool kDoAnalyseForDeadlock = false;
#endif

//-------------------------------------------------------------------------------------------------
// class MutexLockKey

MutexLockKey::MutexLockKey()
:
    line( 0 ),
    mutexPtr( nullptr ),
    lockID( 0 ),
    threadHoldingMutex( 0 ),
    lockRecursionDepth( 0 )
{
}

MutexLockKey::MutexLockKey(
    const char* _sourceFile,
    int _sourceLine,
    QnMutex* _mutexPtr,
    size_t _lockID,
    std::uintptr_t _threadHoldingMutex )
:
    sourceFile( _sourceFile ),
    line( _sourceLine ),
    mutexPtr( _mutexPtr ),
    lockID( _lockID ),
    threadHoldingMutex( _threadHoldingMutex ),
    lockRecursionDepth( 0 )
{
}

bool MutexLockKey::operator<( const MutexLockKey& rhs ) const
{
    //return mutexPtr < rhs.mutexPtr;
    if( sourceFile < rhs.sourceFile )
        return true;
    if( sourceFile > rhs.sourceFile )
        return false;

    if( line < rhs.line )
        return true;
    if( line > rhs.line )
        return false;

    if( mutexPtr < rhs.mutexPtr )
        return true;
    if( mutexPtr > rhs.mutexPtr )
        return false;

    return lockID < rhs.lockID;
}

bool MutexLockKey::operator==( const MutexLockKey& rhs ) const
{
    //return mutexPtr == rhs.mutexPtr;

    return sourceFile == rhs.sourceFile
        && line == rhs.line
        && mutexPtr == rhs.mutexPtr
        && lockID == rhs.lockID;
}

bool MutexLockKey::operator!=( const MutexLockKey& rhs ) const
{
    return !( *this == rhs );
}

QString MutexLockKey::toString() const
{
    return lit( "%1:%2. mutex %3, relock number %4" ).
        arg(QLatin1String(sourceFile)).arg(line).arg((size_t)mutexPtr, 0, 16).arg(lockID);
}

//-------------------------------------------------------------------------------------------------
// class LockGraphEdgeData

LockGraphEdgeData::TwoMutexLockData::TwoMutexLockData()
:
    threadID( 0 )
{
}

LockGraphEdgeData::TwoMutexLockData::TwoMutexLockData(
    std::uintptr_t _threadID,
    MutexLockKey _firstLocked,
    MutexLockKey _secondLocked )
:
    threadID( _threadID ),
    firstLocked( _firstLocked ),
    secondLocked( _secondLocked )
{
}

bool LockGraphEdgeData::TwoMutexLockData::operator<( const TwoMutexLockData& rhs ) const
{
    if( threadID < rhs.threadID )
        return true;
    if( rhs.threadID < threadID )
        return false;

    if( firstLocked < rhs.firstLocked )
        return true;
    if( rhs.firstLocked < firstLocked )
        return false;

    return secondLocked < rhs.secondLocked;
}

bool LockGraphEdgeData::TwoMutexLockData::operator==( const TwoMutexLockData& rhs ) const
{
    return
        threadID == rhs.threadID &&
        firstLocked == rhs.firstLocked &&
        secondLocked == rhs.secondLocked;
}

bool LockGraphEdgeData::TwoMutexLockData::operator!=( const TwoMutexLockData& rhs ) const
{
    return !(*this == rhs);
}


LockGraphEdgeData::LockGraphEdgeData()
{
}

LockGraphEdgeData::LockGraphEdgeData( LockGraphEdgeData&& rhs )
:
    lockPositions( std::move(rhs.lockPositions) )
{
}

LockGraphEdgeData::LockGraphEdgeData( const LockGraphEdgeData& rhs )
:
    lockPositions( rhs.lockPositions )
{
}

LockGraphEdgeData& LockGraphEdgeData::operator=( LockGraphEdgeData&& rhs )
{
    lockPositions = std::move( rhs.lockPositions );
    return *this;
}

bool LockGraphEdgeData::connectedTo( const LockGraphEdgeData& rhs ) const
{
    for( const TwoMutexLockData& lockData: lockPositions )
    {
        //if there is an element in rhs with same thread id and rhs.firstLocked == secondLocked, than connected
        auto iter = rhs.lockPositions.lower_bound( TwoMutexLockData(
            lockData.threadID, lockData.secondLocked, MutexLockKey() ) );
        if( iter != rhs.lockPositions.end() &&
            iter->threadID == lockData.threadID &&
            iter->firstLocked == lockData.secondLocked )
        {
            return true;    //connected
        }
    }

    //note: lockPositions is sorted by threadID

    //two edges are connected if there are elements with different thread id
    return !is_equal_sorted_ranges_if(
        lockPositions.cbegin(), lockPositions.cend(),
        rhs.lockPositions.cbegin(), rhs.lockPositions.cend(),
        []( const TwoMutexLockData& one, const TwoMutexLockData& two ){
            return one.threadID < two.threadID;
        } );
}

//-------------------------------------------------------------------------------------------------
// class ThreadContextPool

ThreadContext& ThreadContextPool::currentThreadContext()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const std::uintptr_t curThreadId = ::currentThreadSystemId();
    auto iter = m_threadIdToContext.find(curThreadId);
    if (iter != m_threadIdToContext.end())
        return iter->second;

    iter = m_threadIdToContext.emplace(curThreadId, ThreadContext()).first;
    return iter->second;
}

void ThreadContextPool::removeCurrentThreadContext()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const std::uintptr_t curThreadId = ::currentThreadSystemId();
    m_threadIdToContext.erase(curThreadId);
}

//-------------------------------------------------------------------------------------------------
// class ThreadContextGuard

ThreadContextGuard::ThreadContextGuard(
    ThreadContextPool* threadContextPool)
    :
    m_threadContextPool(threadContextPool),
    m_threadContext(threadContextPool->currentThreadContext())
{
}

ThreadContextGuard::~ThreadContextGuard()
{
    if (m_threadContext.currentLockPath.empty())
        m_threadContextPool->removeCurrentThreadContext();

    m_threadContextPool = nullptr;
}

ThreadContext* ThreadContextGuard::operator->()
{
    return &m_threadContext;
}

const ThreadContext* ThreadContextGuard::operator->() const
{
    return &m_threadContext;
}

//-------------------------------------------------------------------------------------------------
// class MutexLockAnalyzer

void MutexLockAnalyzer::mutexCreated( QnMutex* const /*mutex*/ )
{
}

void MutexLockAnalyzer::beforeMutexDestruction( QnMutex* const mutex )
{
    if (kDoAnalyseForDeadlock)
    {
        QWriteLocker lk( &m_mutex );
        m_lockDigraph.removeVertice( mutex );
    }
}

void MutexLockAnalyzer::afterMutexLocked( const MutexLockKey& mutexLockPosition )
{
    const auto curThreadId = ::currentThreadSystemId();
    ThreadContextGuard threadContext(&m_threadContextPool);

    if( threadContext->currentLockPath.empty() )
    {
        threadContext->currentLockPath.push_front( mutexLockPosition );
        return;
    }

    const MutexLockKey& prevLock = threadContext->currentLockPath.front();
    if( prevLock.mutexPtr == mutexLockPosition.mutexPtr )
    {
        if( mutexLockPosition.mutexPtr->m_impl->recursive )
        {
            ++threadContext->currentLockPath.front().lockRecursionDepth;
            return;     //ignoring recursive lock
        }

        threadContext->currentLockPath.push_front( mutexLockPosition );
        const QString& deadLockMsg = QString::fromLatin1(
            "Detected deadlock. Double mutex lock. Path:\n"
            "    %1\n").
            arg(pathToString(threadContext->currentLockPath.crbegin(), threadContext->currentLockPath.crend()));

        NX_LOG( deadLockMsg, cl_logALWAYS );
        std::cerr << deadLockMsg.toStdString() << std::endl;
        return;
    }

    threadContext->currentLockPath.push_front( mutexLockPosition );
    if( !kDoAnalyseForDeadlock )
        return;

    QReadLocker readLock( &m_mutex );

    LockGraphEdgeData::TwoMutexLockData curMutexLockData(
        curThreadId,
        prevLock,
        mutexLockPosition );

    const LockGraphEdgeData* existingEdgeData = nullptr;
    if( m_lockDigraph.findEdge( prevLock.mutexPtr, mutexLockPosition.mutexPtr, &existingEdgeData ) &&
        (existingEdgeData->lockPositions.find( curMutexLockData ) != existingEdgeData->lockPositions.end()) )
    {
        //no need to check for deadlock if graph is not modified
        return;
    }

    std::list<QnMutex*> existingPath;
    std::list<LockGraphEdgeData> edgesTravelled;
    //travelling edges containing any thread different from current
    //    This is needed to avoid reporting deadlock in case of loop in the same thread.
    //    Consider following: m1->m2, m2->m1. But both lock happen in the same thread only,
    //    so deadlock is possible.
    //    NOTE: recursive locking is handled separately
    if( m_lockDigraph.findAnyPathIf(
            mutexLockPosition.mutexPtr, prevLock.mutexPtr,
            &existingPath, &edgesTravelled,
            [curThreadId]( const LockGraphEdgeData& edgeData )->bool {
                return std::find_if(
                        edgeData.lockPositions.cbegin(),
                        edgeData.lockPositions.cend(),
                        [curThreadId](const LockGraphEdgeData::TwoMutexLockData& lockData){
                            return lockData.threadID != curThreadId;
                        }
                    ) != edgeData.lockPositions.cend();
            } ) )
    {
        //found path can still be not connected
        //Example: imagine that mtx1 has been locked with mtx4 already locked
        //found edges:
        //    - from {mtx1, line 10} to {mtx2, line 20}, thread 1
        //    - from {mtx2, line 30} to {mtx3, line 40}, thread 1
        //    - from {mtx3, line 50} to {mtx4, line 60}, thread 2
        //Such path is not connected since mtx3 cannot be locked with mtx1 locked with such path.
        //Consider connected path (check mtx2):
        //    - from {mtx1, line 10} to {mtx2, line 20}, thread 1
        //    - from {mtx2, line 20} to {mtx3, line 40}, thread 1
        //    - from {mtx3, line 50} to {mtx4, line 60}, thread 2

        if( pathConnected( edgesTravelled ) )
        {
            //found deadlock between prevLock and mutexLockPosition
            const QString& deadLockMsg = QString::fromLatin1(
                "Detected deadlock between %1 and %2:\n"
                "    Existing path:\n%3\n"
                "    New path:\n%4\n").
                arg(prevLock.toString()).arg(mutexLockPosition.toString()).
                arg(pathToString(edgesTravelled)).
                arg(pathToString(threadContext->currentLockPath.crbegin(), threadContext->currentLockPath.crend()));

            readLock.unlock();

            NX_LOG( deadLockMsg, cl_logALWAYS );
            std::cerr<<deadLockMsg.toStdString()<<std::endl;

            #if defined(_WIN32)
                DebugBreak();
            #elif defined(__linux__)
                kill(getppid(), SIGTRAP);
            #endif
        }
    }

    //upgrading lock
    readLock.unlock();
    QWriteLocker writeLock( &m_mutex );

    //OK to add edge
    std::pair<LockGraphEdgeData*, bool> p = m_lockDigraph.addEdge(
        prevLock.mutexPtr, mutexLockPosition.mutexPtr, LockGraphEdgeData() );
    p.first->lockPositions.insert( std::move(curMutexLockData) );
}

void MutexLockAnalyzer::expectNoLocks()
{
    ThreadContextGuard threadContext(&m_threadContextPool);

    std::vector<MutexLockKey> path;
    for (const auto& key: threadContext->currentLockPath)
    {
        if (!key.mutexPtr->isRecursive())
            path.push_back(key);
    }

    NX_ASSERT(path.empty(), lm("Unexpected mutex locks: \n%1")
        .container(path, QString::fromLatin1("\n"), QString(), QString()));
}

void MutexLockAnalyzer::beforeMutexUnlocked( const MutexLockKey& mutexLockPosition )
{
    ThreadContextGuard threadContext(&m_threadContextPool);

    NX_CRITICAL( !threadContext->currentLockPath.empty() );
    if( threadContext->currentLockPath.front().lockRecursionDepth > 0 )
    {
        --threadContext->currentLockPath.front().lockRecursionDepth;
        return;
    }

    NX_ASSERT( mutexLockPosition == threadContext->currentLockPath.front() );
    threadContext->currentLockPath.pop_front();
}

void MutexLockAnalyzer::threadStarted( std::uintptr_t /*sysThreadID*/ )
{
    //TODO #ak
}

void MutexLockAnalyzer::threadStopped( std::uintptr_t /*sysThreadID*/ )
{
    //TODO #ak
}

Q_GLOBAL_STATIC( MutexLockAnalyzer, MutexLockAnalyzer_instance )

MutexLockAnalyzer* MutexLockAnalyzer::instance()
{
    return MutexLockAnalyzer_instance();
}

QString MutexLockAnalyzer::pathToString( const std::list<LockGraphEdgeData>& edgesTravelled )
{
    MutexLockKey prevSecondLock;
    std::uintptr_t prevLockThreadID = 0;
    QString pathStr;
    for( auto it = edgesTravelled.cbegin(); it != edgesTravelled.cend(); ++it )
    {
        const LockGraphEdgeData& edge = *it;

        NX_ASSERT( !edge.lockPositions.empty() );
        //selecting which lockPositions element to use
        const LockGraphEdgeData::TwoMutexLockData& lockData = *edge.lockPositions.cbegin();

        bool lockStackChanged = false;
        if( it != edgesTravelled.cbegin() )
        {
            if( lockData.firstLocked != prevSecondLock ||
                lockData.threadID != prevLockThreadID )
            {
                lockStackChanged = true;
            }
        }

        if( lockStackChanged )
            pathStr += lit("----------------\n");
        if( lockStackChanged || (it == edgesTravelled.cbegin()) )
            pathStr += lockData.firstLocked.toString() + QLatin1String("\n");
        pathStr += QString::fromLatin1("    thread %1\n").arg(lockData.threadID, 0, 16);
        pathStr += lockData.secondLocked.toString() + QLatin1String("\n");

        prevSecondLock = lockData.secondLocked;
        prevLockThreadID = lockData.threadID;
    }

    return pathStr;
}

bool MutexLockAnalyzer::pathConnected( const std::list<LockGraphEdgeData>& edgesTravelled ) const
{
    const LockGraphEdgeData* prevEdgeData = nullptr;
    for( const LockGraphEdgeData& edgeData: edgesTravelled )
    {
        if( prevEdgeData )
        {
            if( !prevEdgeData->connectedTo( edgeData ) )
                return false;
        }

        prevEdgeData = &edgeData;
    }

    return true;
}

#endif  //USE_OWN_MUTEX
