// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <mutex>
#include <shared_mutex>
#include <condition_variable>

#include <nx/utils/std/optional.h>

#include "mutex.h"

namespace nx {

class NX_UTILS_API MutexStdDelegate: public MutexDelegate
{
public:
    MutexStdDelegate(Mutex::RecursionMode mode);

    virtual void lock(const char* sourceFile, int sourceLine, int lockId) override;
    virtual bool tryLock(const char* sourceFile, int sourceLine, int lockId) override;

    virtual void unlock() override;
    virtual bool isRecursive() const override;

private:
    friend class WaitConditionStdDelegate;
    std::unique_ptr<std::mutex> m_mutex;
    std::unique_ptr<std::recursive_mutex> m_recursiveMutex;
};

class NX_UTILS_API ReadWriteLockStdDelegate: public ReadWriteLockDelegate
{
public:
    ReadWriteLockStdDelegate(ReadWriteLock::RecursionMode mode);

    virtual void lockForRead(const char* sourceFile, int sourceLine, int lockId) override;
    virtual void lockForWrite(const char* sourceFile, int sourceLine, int lockId) override;

    virtual bool tryLockForRead(const char* sourceFile, int sourceLine, int lockId) override;
    virtual bool tryLockForWrite(const char* sourceFile, int sourceLine, int lockId) override;

    virtual void unlock() override;

private:
    friend class WaitConditionStdDelegate;
    std::unique_ptr<std::shared_mutex> m_mutex;
    std::unique_ptr<std::recursive_mutex> m_recursiveMutex;
};

class NX_UTILS_API WaitConditionStdDelegate: public WaitConditionDelegate
{
public:
    virtual bool wait(MutexDelegate* mutex, std::chrono::milliseconds timeout) override;
    virtual void wakeAll() override;
    virtual void wakeOne() override;

private:
    std::condition_variable m_condition;
};

} // namespace nx
