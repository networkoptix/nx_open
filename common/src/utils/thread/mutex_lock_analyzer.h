/**********************************************************
* 13 feb 2015
* akolesnikov@networkoptix.com
***********************************************************/

#ifdef USE_OWN_MUTEX

#ifndef MUTEX_LOCK_ANALYZER_H
#define MUTEX_LOCK_ANALYZER_H

#include <cstdint>
#include <deque>
#include <map>

#include <QtCore/QReadWriteLock>
#include <QtCore/QString>

#include <utils/math/digraph.h>


//!Checks that two sorted ranges contain elements of same value using comparator \a comp. Though, different element count is allowed
/*!
    \note Multiple same elements in either range are allowed
    \param Comp comparator used to sort ranges
*/
template<class FirstRangeIter, class SecondRangeIter, class Comp>
bool is_equal_sorted_ranges_if(
    const FirstRangeIter& firstRangeBegin, const FirstRangeIter& firstRangeEnd,
    const SecondRangeIter& secondRangeBegin, const SecondRangeIter& secondRangeEnd,
    Comp comp )
{
    FirstRangeIter fIt = firstRangeBegin;
    SecondRangeIter sIt = secondRangeBegin;
    for( ; (fIt != firstRangeEnd) || (sIt != secondRangeEnd); )
    {
        if( ((fIt == firstRangeEnd) != (sIt == secondRangeEnd)) ||
            (comp(*fIt, *sIt) || comp(*sIt, *fIt)) )    //*fIt != *sIt
        {
            return false;
        }

        ++fIt;
        //iterating second range while *sIt < *fIt
        const auto& prevSVal = *sIt;
        while( (sIt != secondRangeEnd) &&
               ((sIt == secondRangeEnd) < (fIt == firstRangeEnd) || comp(*sIt, *fIt)) )
        {
            if( comp(prevSVal, *sIt) || comp(*sIt, prevSVal) )   //prevSVal != *sIt
                return false;   //second range element changed value but still less than first range element
            ++sIt;
        }
    }

    return true;
}

/*!
    \note Multiple same elements in either range are allowed
*/
template<class FirstRangeIter, class SecondRangeIter>
bool are_elements_same_in_sorted_ranges(
    const FirstRangeIter& firstRangeBegin, const FirstRangeIter& firstRangeEnd,
    const SecondRangeIter& secondRangeBegin, const SecondRangeIter& secondRangeEnd )
{
    FirstRangeIter fIt = firstRangeBegin;
    SecondRangeIter sIt = secondRangeBegin;
    for( ; (fIt != firstRangeEnd) || (sIt != secondRangeEnd); )
    {
        if( ((fIt == firstRangeEnd) != (sIt == secondRangeEnd)) ||
            (*fIt != *sIt) )
        {
            return false;
        }

        ++fIt;
        //iterating second range while *sIt < *fIt
        const auto& prevSVal = *sIt;
        while( (sIt != secondRangeEnd) &&
               ((sIt == secondRangeEnd) < (fIt == firstRangeEnd) || (*sIt < *fIt)) )
        {
            if( *sIt != prevSVal )
                return false;   //second range element changed value but still less than first range element
            ++sIt;
        }
    }

    return true;
}

class QnMutex;

class MutexLockKey
{
public:
    QByteArray sourceFile;
    int line;
    QnMutex* mutexPtr;
    int lockID;
    std::uintptr_t threadHoldingMutex;
    int lockRecursionDepth;

    MutexLockKey();
    MutexLockKey(
        const char* sourceFile,
        int sourceLine,
        QnMutex* mutexPtr,
        int lockID,
        std::uintptr_t threadHoldingMutex );

    bool operator<( const MutexLockKey& rhs ) const;
    bool operator==( const MutexLockKey& rhs ) const;
    bool operator!=( const MutexLockKey& rhs ) const;

    QString toString() const;
};

class LockGraphEdgeData
{
public:
    class TwoMutexLockData
    {
    public:
        //!ID of threads, mutex is being locked in
        std::uintptr_t threadID;
        MutexLockKey firstLocked;
        MutexLockKey secondLocked;

        TwoMutexLockData();
        TwoMutexLockData(
            std::uintptr_t threadID,
            MutexLockKey firstLocked,
            MutexLockKey secondLocked );

        bool operator<( const TwoMutexLockData& rhs ) const;
        bool operator==( const TwoMutexLockData& rhs ) const;
        bool operator!=( const TwoMutexLockData& rhs ) const;
    };

    //!Information about mutex lock
    std::set<TwoMutexLockData> lockPositions;

    LockGraphEdgeData();
    LockGraphEdgeData( LockGraphEdgeData&& rhs );

    LockGraphEdgeData& operator=( LockGraphEdgeData&& rhs );

    /*!
        Returns \a true if run-time transition from *this to \a rhs is possible.
        That means:\n
            - rhs has elements with different thread id
            - or it has element with same thread id and this->secondLocked == rhs.firstLocked
    */
    bool connectedTo( const LockGraphEdgeData& rhs ) const;
};

class MutexLockAnalyzer
{
public:
    void mutexCreated( QnMutex* const mutex );
    void beforeMutexDestruction( QnMutex* const mutex );

    void afterMutexLocked( const MutexLockKey& mutexLockPosition );
    void beforeMutexUnlocked( const MutexLockKey& mutexLockPosition );

    //!Should be called just after a new thread has been started
    void threadStarted( std::uintptr_t sysThreadID );
    //!Should be called just before a thread descriptor is freed
    void threadStopped( std::uintptr_t sysThreadID );

    static MutexLockAnalyzer* instance();

private:
    struct ThreadContext
    {
        std::deque<MutexLockKey> currentLockPath;
    };

    typedef Digraph<QnMutex*, LockGraphEdgeData> MutexLockDigraph;

    mutable QReadWriteLock m_mutex;
    MutexLockDigraph m_lockDigraph;
    //!map<threadId, stack<mutex lock position>>
    std::map<std::uintptr_t, ThreadContext> m_threadContext;

    template<class _Iter>
    QString pathToString( const _Iter& pathStart, const _Iter& pathEnd )
    {
        QString pathStr;

        for( _Iter it = pathStart; it != pathEnd; ++it )
        {
            if( it != pathStart )
                pathStr += QString::fromLatin1("    thread %1\n").arg(it->threadHoldingMutex, 0, 16);
            pathStr += it->toString() + QLatin1String("\n");
        }

        return pathStr;
    }

    QString pathToString( const std::list<LockGraphEdgeData>& edgesTravelled );
    bool pathConnected( const std::list<LockGraphEdgeData>& edgesTravelled ) const;
};

#endif  //MUTEX_LOCK_ANALYZER_H

#endif  //USE_OWN_MUTEX
