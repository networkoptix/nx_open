#pragma once

#include <stack>

#include <QMutex>
#include <QWaitCondition>

#include "mutex.h"
#include "mutex_lock_analyzer.h"

namespace nx::utils {

class NX_UTILS_API MutexDebugDelegate: public MutexDelegate
{
public:
    MutexDebugDelegate(Mutex::RecursionMode mode, bool isAnalyzerInUse);
    ~MutexDebugDelegate();

    virtual void lock(const char* sourceFile, int sourceLine, int lockId) override;
    virtual bool tryLock(const char* sourceFile, int sourceLine, int lockId) override;

    virtual void unlock() override;
    virtual bool isRecursive() const override;

private:
    friend class WaitConditionDebugDelegate;
    void afterLock(const char* sourceFile, int sourceLine, int lockId);
    void beforeUnlock();

private:
    QMutex m_mutex;
    const bool m_isAnalyzerInUse = false;
    std::uintptr_t threadHoldingMutex;
    size_t recursiveLockCount = 0;
    std::stack<MutexLockKey> currentLockStack; //< For recursive mutexes.
};

class NX_UTILS_API ReadWriteLockDebugDelegate: public ReadWriteLockDelegate
{
public:
    ReadWriteLockDebugDelegate(ReadWriteLock::RecursionMode mode, bool isAnalyzerInUse);

    virtual void lockForRead(const char* sourceFile, int sourceLine, int lockId) override;
    virtual void lockForWrite(const char* sourceFile, int sourceLine, int lockId) override;

    virtual bool tryLockForRead(const char* sourceFile, int sourceLine, int lockId) override;
    virtual bool tryLockForWrite(const char* sourceFile, int sourceLine, int lockId) override;

    virtual void unlock() override;

private:
    friend class WaitConditionDebugDelegate;
    MutexDebugDelegate m_delegate;
};

class NX_UTILS_API WaitConditionDebugDelegate: public WaitConditionDelegate
{
public:
    virtual bool wait(MutexDelegate* mutex, std::chrono::milliseconds timeout) override;
    virtual bool wait(ReadWriteLockDelegate* mutex, std::chrono::milliseconds timeout) override;

    virtual void wakeAll() override;
    virtual void wakeOne() override;

private:
    QWaitCondition m_delegate;
};

} // namespace nx::utils

