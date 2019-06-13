#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <mutex>

#include <QtCore/QReadWriteLock>
#include <QtCore/QString>

#include <nx/utils/math/digraph.h>

namespace nx::utils {

class MutexDelegate;

class NX_UTILS_API MutexLockKey
{
public:
    QByteArray sourceFile;
    int line = 0;
    MutexDelegate* mutexPtr = nullptr;
    size_t lockID = 0;
    std::uintptr_t threadHoldingMutex = 0;
    int lockRecursionDepth = 0;
    bool recursive = false;

public:
    MutexLockKey();

    MutexLockKey(
        const char* sourceFile,
        int sourceLine,
        MutexDelegate* mutexPtr,
        size_t lockID,
        std::uintptr_t threadHoldingMutex,
        bool recursive);

    bool operator<(const MutexLockKey& rhs) const;
    bool operator==(const MutexLockKey& rhs) const;
    bool operator!=(const MutexLockKey& rhs) const;

    QString toString() const;
};

class NX_UTILS_API LockGraphEdgeData
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
            MutexLockKey secondLocked);

        bool operator<(const TwoMutexLockData& rhs) const;
        bool operator==(const TwoMutexLockData& rhs) const;
        bool operator!=(const TwoMutexLockData& rhs) const;
    };

    //!Information about mutex lock
    std::set<TwoMutexLockData> lockPositions;

    LockGraphEdgeData();
    LockGraphEdgeData(LockGraphEdgeData&& rhs);
    LockGraphEdgeData(const LockGraphEdgeData& rhs);

    LockGraphEdgeData& operator=(LockGraphEdgeData&& rhs);

    /*!
    Returns true if run-time transition from *this to rhs is possible.
    That means:\n
    - rhs has elements with different thread id
    - or it has element with same thread id and this->secondLocked == rhs.firstLocked
    */
    bool connectedTo(const LockGraphEdgeData& rhs) const;
};

//-------------------------------------------------------------------------------------------------

struct ThreadContext
{
    std::deque<MutexLockKey> currentLockPath;
};

class NX_UTILS_API ThreadContextPool
{
public:
    ThreadContext & currentThreadContext();
    void removeCurrentThreadContext();

private:
    //!map<threadId, stack<mutex lock position>>
    std::map<std::uintptr_t, ThreadContext> m_threadIdToContext;
    std::mutex m_mutex;
};

class NX_UTILS_API ThreadContextGuard
{
public:
    ThreadContextGuard(ThreadContextPool*);
    ~ThreadContextGuard();

    ThreadContextGuard(const ThreadContextGuard&) = delete;
    ThreadContextGuard& operator=(const ThreadContextGuard&) = delete;
    ThreadContextGuard(ThreadContextGuard&&) = delete;
    ThreadContextGuard& operator=(ThreadContextGuard&&) = delete;

    ThreadContext* operator->();
    const ThreadContext* operator->() const;

private:
    ThreadContextPool * m_threadContextPool;
    ThreadContext& m_threadContext;
};

//-------------------------------------------------------------------------------------------------

class NX_UTILS_API MutexLockAnalyzer
{
public:
    using DeadlockDetectedHandler = std::function<void()>;

    MutexLockAnalyzer();

    void mutexCreated(MutexDelegate* const mutex);
    void beforeMutexDestruction(MutexDelegate* const mutex);

    void afterMutexLocked(const MutexLockKey& mutexLockPosition);
    void beforeMutexUnlocked(const MutexLockKey& mutexLockPosition);

    void expectNoLocks();

    /**
     * By default, deadlock results in a process interrupt.
     * @param handler If nullptr, then default handler is installed.
     */
    void setDeadlockDetectedHandler(DeadlockDetectedHandler handler);

    static MutexLockAnalyzer* instance();

private:
    using MutexLockDigraph = Digraph<MutexDelegate*, LockGraphEdgeData>;

    mutable QReadWriteLock m_mutex;
    MutexLockDigraph m_lockDigraph;
    ThreadContextPool m_threadContextPool;
    DeadlockDetectedHandler m_deadlockDetectedHandler;

    template<class _Iter>
    QString pathToString(const _Iter& pathStart, const _Iter& pathEnd)
    {
        QString pathStr;

        for (_Iter it = pathStart; it != pathEnd; ++it)
        {
            if (it != pathStart)
                pathStr += QString::fromLatin1("    thread %1\n").arg(it->threadHoldingMutex, 0, 16);
            pathStr += it->toString() + QLatin1String("\n");
        }

        return pathStr;
    }

    QString pathToString(const std::list<LockGraphEdgeData>& edgesTravelled);
    bool pathConnected(const std::list<LockGraphEdgeData>& edgesTravelled) const;
};

} // namespace nx::utils
