#pragma once

#include <memory>
#include <chrono>

#include "mutex_locker.h"

namespace nx::utils {

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
    struct NX_CONCATENATE(NxUtilsMutexLocker, __LINE__): public ::nx::utils::MutexLocker \
    { \
        NX_CONCATENATE(NxUtilsMutexLocker, __LINE__)(::nx::utils::Mutex* mutex): \
            ::nx::utils::MutexLocker(mutex, __FILE__, __LINE__) {}  \
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
    struct NX_CONCATENATE(NxUtilsReadLocker, __LINE__): public ::nx::utils::ReadLocker \
    { \
        NX_CONCATENATE(NxUtilsReadLocker, __LINE__)(::nx::utils::ReadWriteLock* mutex): \
            ::nx::utils::ReadLocker(mutex, __FILE__, __LINE__) {} \
    }

class NX_UTILS_API WriteLocker: public Locker<ReadWriteLock>
{
public:
    WriteLocker(ReadWriteLock* mutex, const char* sourceFile, int sourceLine);
};

#define NX_WRITE_LOCKER \
    struct NX_CONCATENATE(NxUtilsWriteLocker, __LINE__): public ::nx::utils::WriteLocker \
    { \
        NX_CONCATENATE(NxUtilsWriteLocker, __LINE__)(::nx::utils::ReadWriteLock* mutex): \
            ::nx::utils::WriteLocker(mutex, __FILE__, __LINE__) {} \
    }

//-------------------------------------------------------------------------------------------------

class NX_UTILS_API WaitConditionDelegate
{
public:
    virtual ~WaitConditionDelegate() = default;

    virtual bool wait(MutexDelegate* mutex, std::chrono::milliseconds timeout) = 0;
    virtual bool wait(ReadWriteLockDelegate* mutex, std::chrono::milliseconds timeout) = 0;

    virtual void wakeAll() = 0;
    virtual void wakeOne() = 0;
};

class NX_UTILS_API WaitCondition
{
public:
    WaitCondition();

    bool wait(Mutex* mutex, std::chrono::milliseconds timeout = std::chrono::milliseconds::max());
    bool wait(Mutex* mutex, unsigned long timeout); //< TODO: Remove with usages.

    bool wait(ReadWriteLock* mutex, std::chrono::milliseconds timeout = std::chrono::milliseconds::max());
    bool wait(ReadWriteLock* mutex, unsigned long timeout); //< TODO: Remove with usages.

    void wakeAll();
    void wakeOne();

private:
    std::unique_ptr<WaitConditionDelegate> m_delegate;
};

} // namespace nx::utils

//-------------------------------------------------------------------------------------------------

// Remove as soon as all usages are fixed:

// TODO: Remove with all usages.
using QnMutex = nx::utils::Mutex;
using QnMutexLockerBase = nx::utils::Locker<nx::utils::Mutex>;
using QnMutexUnlocker = nx::utils::Unlocker<nx::utils::Mutex>;
#define QnMutexLocker NX_MUTEX_LOCKER

// TODO: Remove with all usages.
using QnReadWriteLock = nx::utils::ReadWriteLock;
#define QnReadLocker NX_READ_LOCKER
#define QnWriteLocker NX_WRITE_LOCKER
