#pragma once

#include <QMutex>
#include <QReadWriteLock>
#include <QWaitCondition>

#include "mutex.h"

namespace nx::utils {

class NX_UTILS_API MutexQtDelegate: public MutexDelegate
{
public:
    MutexQtDelegate(Mutex::RecursionMode mode);

    virtual void lock(const char* sourceFile, int sourceLine, int lockId) override;
    virtual bool tryLock(const char* sourceFile, int sourceLine, int lockId) override;

    virtual void unlock() override;
    virtual bool isRecursive() const override;

private:
    friend class WaitConditionQtDelegate;
    QMutex m_delegate;
};

class NX_UTILS_API ReadWriteLockQtDelegate: public ReadWriteLockDelegate
{
public:
    ReadWriteLockQtDelegate(ReadWriteLock::RecursionMode mode);

    virtual void lockForRead(const char* sourceFile, int sourceLine, int lockId) override;
    virtual void lockForWrite(const char* sourceFile, int sourceLine, int lockId) override;

    virtual bool tryLockForRead(const char* sourceFile, int sourceLine, int lockId) override;
    virtual bool tryLockForWrite(const char* sourceFile, int sourceLine, int lockId) override;

    virtual void unlock() override;

private:
    friend class WaitConditionQtDelegate;
    QReadWriteLock m_delegate;
};

class NX_UTILS_API WaitConditionQtDelegate: public WaitConditionDelegate
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
