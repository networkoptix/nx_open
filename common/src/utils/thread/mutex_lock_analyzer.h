/**********************************************************
* 13 feb 2015
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef MUTEX_LOCK_ANALYZER_H
#define MUTEX_LOCK_ANALYZER_H

#include <cstdint>
#include <deque>
#include <map>

#include <QtCore/QMutex>
#include <QtCore/QString>

#include <utils/math/digraph.h>


class MutexLockKey
{
public:
    QByteArray sourceFile;
    int line;
    void* mutexPtr;
    int lockID;

    MutexLockKey();
    MutexLockKey(
        const char* sourceFile,
        int sourceLine,
        void* mutexPtr,
        int lockID );

    bool operator<( const MutexLockKey& rhs ) const;
    bool operator==( const MutexLockKey& rhs ) const;

    QString toString() const;
};

class MutexLockAnalyzer
{
public:
    void afterMutexLocked( const MutexLockKey& mutexLockPosition );
    void beforeMutexUnlocked( const MutexLockKey& mutexLockPosition );

    static MutexLockAnalyzer* instance();

private:
    mutable QMutex m_mutex;
    Digraph<MutexLockKey> m_lockDigraph;
    //!map<threadId, stack<mutex lock position>>
    std::map<std::uintptr_t, std::deque<MutexLockKey>> m_currentLockPathByThread;

    template<class _Iter>
    QString pathToString( const _Iter& pathStart, const _Iter& /*pathEnd*/ )
    {
        //TODO
        return pathStart->toString();
    }
};

#endif  //MUTEX_LOCK_ANALYZER_H
