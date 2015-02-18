/**********************************************************
* 13 feb 2015
* akolesnikov@networkoptix.com
***********************************************************/

#ifdef USE_OWN_MUTEX

#include "mutex_lock_analyzer.h"

#include <QtGlobal>
#include <QtCore/QMutexLocker>

#include <utils/common/log.h>
#include <utils/common/long_runnable.h>


////////////////////////////////////////////////////////////
//// class MutexLockKey
////////////////////////////////////////////////////////////

MutexLockKey::MutexLockKey()
:
    line( 0 ),
    mutexPtr( nullptr ),
    lockID( 0 )
{
}

MutexLockKey::MutexLockKey(
    const char* _sourceFile,
    int _sourceLine,
    void* _mutexPtr,
    int _lockID )
:
    sourceFile( _sourceFile ),
    line( _sourceLine ),
    mutexPtr( _mutexPtr ),
    lockID( _lockID )
{
}

bool MutexLockKey::operator<( const MutexLockKey& rhs ) const
{
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
    return sourceFile == rhs.sourceFile
        && line == rhs.line
        && mutexPtr == rhs.mutexPtr
        && lockID == rhs.lockID;
}

QString MutexLockKey::toString() const
{
    return lit( "%1:%2. mutex %3, relock number %4" ).
        arg(QLatin1String(sourceFile)).arg(line).arg((size_t)mutexPtr, 0, 16).arg(lockID);
}


////////////////////////////////////////////////////////////
//// class MutexLockAnalyzer
////////////////////////////////////////////////////////////

void MutexLockAnalyzer::afterMutexLocked( const MutexLockKey& mutexLockPosition )
{
    QMutexLocker lk( &m_mutex );
    
    std::deque<MutexLockKey>& currentThreadPath = 
        m_currentLockPathByThread[QnLongRunnable::currentThreadSystemId()];
    if( currentThreadPath.empty() )
    {
        currentThreadPath.push_front( mutexLockPosition );
        return;
    }

    const MutexLockKey& prevLock = currentThreadPath.front();
    currentThreadPath.push_front( mutexLockPosition );
    std::list<MutexLockKey> existingPath;
    const Digraph<MutexLockKey>::Result res = m_lockDigraph.addEdge(
        currentThreadPath.front(), mutexLockPosition, &existingPath );
    if( res != Digraph<MutexLockKey>::loopDetected )
        return;

    //found deadlock between currentThreadPath.top() and mutexLockPosition
    const QString& deadLockMsg = QString::fromLatin1(
        "Detected deadlock between %1 and %2:\n"
        "    First path:\n%3\n"
        "    Second path:\n%4\n").
        arg(prevLock.toString()).arg(mutexLockPosition.toString()).
        arg(pathToString(currentThreadPath.crbegin(), currentThreadPath.crend())).
        arg(pathToString(existingPath.cbegin(), existingPath.cend()));

    lk.unlock();

    NX_LOG( deadLockMsg, cl_logALWAYS );
    assert( false );
}

void MutexLockAnalyzer::beforeMutexUnlocked( const MutexLockKey& mutexLockPosition )
{
    QMutexLocker lk( &m_mutex );

    std::deque<MutexLockKey>& currentThreadPath = 
        m_currentLockPathByThread[QnLongRunnable::currentThreadSystemId()];
    assert( !currentThreadPath.empty() );
    assert( mutexLockPosition == currentThreadPath.front() );
    currentThreadPath.pop_front();
}

Q_GLOBAL_STATIC( MutexLockAnalyzer, MutexLockAnalyzer_instance )

MutexLockAnalyzer* MutexLockAnalyzer::instance()
{
    return MutexLockAnalyzer_instance();
}

#endif  //USE_OWN_MUTEX
