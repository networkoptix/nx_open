/**********************************************************
* 13 feb 2015
* akolesnikov@networkoptix.com
***********************************************************/

#ifdef USE_OWN_MUTEX

#include "mutex_lock_analyzer.h"

#include <iostream>

#include <QtGlobal>
#include <QtCore/QMutexLocker>

#include <utils/common/log.h>
#include <utils/common/long_runnable.h>

#include "mutex.h"
#include "mutex_impl.h"


////////////////////////////////////////////////////////////
//// class MutexLockKey
////////////////////////////////////////////////////////////

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
    int _lockID,
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

////////////////////////////////////////////////////////////
//// class LockGraphEdgeData
////////////////////////////////////////////////////////////
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
    return !are_elements_same_in_sorted_ranges_if(
        lockPositions.cbegin(), lockPositions.cend(),
        rhs.lockPositions.cbegin(), rhs.lockPositions.cend(),
        []( const TwoMutexLockData& one, const TwoMutexLockData& two ){
            return one.threadID < two.threadID;
        } );
}


////////////////////////////////////////////////////////////
//// class MutexLockAnalyzer
////////////////////////////////////////////////////////////

void MutexLockAnalyzer::mutexCreated( QnMutex* const /*mutex*/ )
{
}

static std::uintptr_t lockedThread = 0;

void MutexLockAnalyzer::beforeMutexDestruction( QnMutex* const mutex )
{
    QMutexLocker lk( &m_mutex );

    lockedThread = QnLongRunnable::currentThreadSystemId();

    m_lockDigraph.removeVertice( mutex );
}

void MutexLockAnalyzer::afterMutexLocked( const MutexLockKey& mutexLockPosition )
{
    QMutexLocker lk( &m_mutex );

    lockedThread = QnLongRunnable::currentThreadSystemId();

    const std::uintptr_t curThreadID = QnLongRunnable::currentThreadSystemId();
    ThreadContext& threadContext = m_threadContext[curThreadID];
    
    if( threadContext.currentLockPath.empty() )
    {
        threadContext.currentLockPath.push_front( mutexLockPosition );
        return;
    }

    const MutexLockKey& prevLock = threadContext.currentLockPath.front();
    if( prevLock.mutexPtr == mutexLockPosition.mutexPtr )
    {
        if( mutexLockPosition.mutexPtr->m_impl->recursive )
        {
            ++threadContext.currentLockPath.front().lockRecursionDepth;
            return;     //ignoring recursive lock
        }
        
        threadContext.currentLockPath.push_front( mutexLockPosition );
        const QString& deadLockMsg = QString::fromLatin1(
            "Detected deadlock. Double mutex lock. Path:\n"
            "    %1\n").
            arg(pathToString(threadContext.currentLockPath.crbegin(), threadContext.currentLockPath.crend()));

        lk.unlock();

        NX_LOG( deadLockMsg, cl_logALWAYS );
        std::cerr<<deadLockMsg.toStdString()<<std::endl;
        assert( false );
        return;
    }

    threadContext.currentLockPath.push_front( mutexLockPosition );

    LockGraphEdgeData::TwoMutexLockData curMutexLockData( 
        curThreadID,
        prevLock,
        mutexLockPosition );

    LockGraphEdgeData* existingEdgeData = nullptr;
    if( m_lockDigraph.findEdge( prevLock.mutexPtr, mutexLockPosition.mutexPtr, &existingEdgeData ) &&
        (existingEdgeData->lockPositions.find( curMutexLockData ) != existingEdgeData->lockPositions.end()) )
    {
        //no need to check for dead lock if graph is not modified
        return;
    }

    std::list<QnMutex*> existingPath;
    std::list<LockGraphEdgeData> edgesTravelled;
    //travelling edges containing any thread different from current
    if( m_lockDigraph.findAnyPathIf(
            mutexLockPosition.mutexPtr, prevLock.mutexPtr,
            &existingPath, &edgesTravelled,
            [curThreadID]( const LockGraphEdgeData& edgeData )->bool {
                return std::find_if(
                        edgeData.lockPositions.cbegin(),
                        edgeData.lockPositions.cend(),
                        [curThreadID](const LockGraphEdgeData::TwoMutexLockData& lockData){
                            return lockData.threadID != curThreadID;
                        }
                    ) != edgeData.lockPositions.cend();
            } ) )
    {
        //found path can still be not connected
        //Example (returned edges):
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
                arg(pathToString(threadContext.currentLockPath.crbegin(), threadContext.currentLockPath.crend()));

            lk.unlock();

            NX_LOG( deadLockMsg, cl_logALWAYS );
            std::cerr<<deadLockMsg.toStdString()<<std::endl;
            assert( false );
        }
        
    }

    //OK to add edge
    std::pair<LockGraphEdgeData*, bool> p = m_lockDigraph.addEdge(
        prevLock.mutexPtr, mutexLockPosition.mutexPtr, LockGraphEdgeData() );
    p.first->lockPositions.insert( std::move(curMutexLockData) );
}

void MutexLockAnalyzer::beforeMutexUnlocked( const MutexLockKey& mutexLockPosition )
{
    QMutexLocker lk( &m_mutex );

    lockedThread = QnLongRunnable::currentThreadSystemId();

    ThreadContext& threadContext = m_threadContext[QnLongRunnable::currentThreadSystemId()];

    assert( !threadContext.currentLockPath.empty() );
    if( threadContext.currentLockPath.front().lockRecursionDepth > 0 )
    {
        --threadContext.currentLockPath.front().lockRecursionDepth;
        return;
    }
    assert( mutexLockPosition == threadContext.currentLockPath.front() );
    threadContext.currentLockPath.pop_front();
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

        assert( !edge.lockPositions.empty() );
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
