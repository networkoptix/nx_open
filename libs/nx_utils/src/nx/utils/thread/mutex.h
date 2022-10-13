// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <chrono>

#include "mutex_locker.h"

namespace nx {

class NX_UTILS_API MutexDelegate
{
public:

    virtual ~MutexDelegate() = default;

    virtual void lock(const char* sourceFile, int sourceLine, int lockId) = 0;
    virtual bool tryLock(const char* sourceFile, int sourceLine, int lockId) = 0;

    virtual void unlock() = 0;
    virtual bool isRecursive() const = 0;
};

class NX_UTILS_API Mutex
{
public:
    enum RecursionMode { Recursive, NonRecursive };
    Mutex(RecursionMode mode = NonRecursive);

    void lock(const char* sourceFile = 0, int sourceLine = 0, int lockId = 0);
    bool tryLock(const char* sourceFile = 0, int sourceLine = 0, int lockId = 0);
    bool try_lock() { return tryLock(); }

    void unlock();
    bool isRecursive() const;

private:
    friend class WaitCondition;
    std::unique_ptr<MutexDelegate> m_delegate;
};

class NX_UTILS_API MutexLocker: public Locker<Mutex>
{
public:
    MutexLocker(Mutex* mutex, const char* sourceFile, int sourceLine);
};

#define NX_MUTEX_LOCKER \
    struct NX_CONCATENATE(NxUtilsMutexLocker, __LINE__): public ::nx::MutexLocker \
    { \
        NX_CONCATENATE(NxUtilsMutexLocker, __LINE__)(::nx::Mutex* mutex): \
            ::nx::MutexLocker(mutex, __FILE__, __LINE__) {}  \
    }

//-------------------------------------------------------------------------------------------------

class NX_UTILS_API ReadWriteLockDelegate
{
public:
    virtual ~ReadWriteLockDelegate() = default;

    virtual void lockForRead(const char* sourceFile, int sourceLine, int lockId) = 0;
    virtual void lockForWrite(const char* sourceFile, int sourceLine, int lockId) = 0;

    virtual bool tryLockForRead(const char* sourceFile, int sourceLine, int lockId) = 0;
    virtual bool tryLockForWrite(const char* sourceFile, int sourceLine, int lockId) = 0;

    virtual void unlock() = 0;
};

class NX_UTILS_API ReadWriteLock
{
public:
    enum RecursionMode { Recursive, NonRecursive };
    ReadWriteLock(RecursionMode mode = NonRecursive);

    void lockForRead(const char* sourceFile = 0, int sourceLine = 0, int lockId = 0);
    void lockForWrite(const char* sourceFile = 0, int sourceLine = 0, int lockId = 0);

    bool tryLockForRead(const char* sourceFile = 0, int sourceLine = 0, int lockId = 0);
    bool tryLockForWrite(const char* sourceFile = 0, int sourceLine = 0, int lockId = 0);

    void unlock();

private:
    friend class WaitCondition;
    std::unique_ptr<ReadWriteLockDelegate> m_delegate;
};

class NX_UTILS_API ReadLocker: public Locker<ReadWriteLock>
{
public:
    ReadLocker(ReadWriteLock* mutex, const char* sourceFile, int sourceLine);
};

#define NX_READ_LOCKER \
    struct NX_CONCATENATE(NxUtilsReadLocker, __LINE__): public ::nx::ReadLocker \
    { \
        NX_CONCATENATE(NxUtilsReadLocker, __LINE__)(::nx::ReadWriteLock* mutex): \
            ::nx::ReadLocker(mutex, __FILE__, __LINE__) {} \
    }

class NX_UTILS_API WriteLocker: public Locker<ReadWriteLock>
{
public:
    WriteLocker(ReadWriteLock* mutex, const char* sourceFile, int sourceLine);
};

#define NX_WRITE_LOCKER \
    struct NX_CONCATENATE(NxUtilsWriteLocker, __LINE__): public ::nx::WriteLocker \
    { \
        NX_CONCATENATE(NxUtilsWriteLocker, __LINE__)(::nx::ReadWriteLock* mutex): \
            ::nx::WriteLocker(mutex, __FILE__, __LINE__) {} \
    }

//-------------------------------------------------------------------------------------------------

class NX_UTILS_API WaitConditionDelegate
{
public:
    virtual ~WaitConditionDelegate() = default;

    virtual bool wait(MutexDelegate* mutex, std::chrono::milliseconds timeout) = 0;
    virtual void wakeAll() = 0;
    virtual void wakeOne() = 0;
};

class NX_UTILS_API WaitCondition
{
public:
    WaitCondition();

    bool wait(Mutex* mutex, std::chrono::milliseconds timeout = std::chrono::milliseconds::max());
    bool wait(Mutex* mutex, unsigned long timeout); //< TODO: Remove with usages.

    /**
     * Waits until pred() returns true or timeout passes.
     * @return true if pred() returned true. false if timeout has passed.
     */
    template<typename Pred>
    bool waitFor(
        Mutex* mutex,
        std::chrono::milliseconds timeout,
        Pred pred)
    {
        using namespace std::chrono;

        if (timeout == std::chrono::milliseconds::max())
        {
            while (!pred())
                wait(mutex, timeout);
        }
        else
        {
            const auto deadline = steady_clock::now() + timeout;
            while (!pred())
            {
                timeout = duration_cast<milliseconds>(deadline - steady_clock::now());
                if (timeout <= milliseconds::zero())
                    return false;

                wait(mutex, timeout);
            }
        }

        return true;
    }

    void wakeAll();
    void wakeOne();

private:
    std::unique_ptr<WaitConditionDelegate> m_delegate;
};

} // namespace nx
