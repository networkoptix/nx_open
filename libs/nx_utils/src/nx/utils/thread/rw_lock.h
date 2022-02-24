// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/thread/semaphore.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace utils {

enum class RwLockType
{
    read,
    write,
};

/**
 * Read write lock. The write lock is always exclusive (no simultaneous reads and writes allowed)
 * Maximum number of readers is defined by user.
 */
class NX_UTILS_API RwLock final
{
public:
    explicit RwLock(int maxNumberOfReadLocks);
    ~RwLock() = default;

    RwLock(const RwLock&) = delete;
    void operator=(const RwLock&) = delete;

    void lock(RwLockType lockType);
    void unlock(RwLockType lockType);

private:
    int m_maxNumberOfReadLocks;
    Semaphore m_readLock;
    nx::Mutex m_writeLock;
};

/**
 * RAII wrapper for RwLock
 */
class NX_UTILS_API RwLocker
{
public:
    explicit RwLocker(RwLock* lock, RwLockType locktype);
    ~RwLocker();

    RwLocker(const RwLocker&) = delete;
    void operator=(const RwLocker&) = delete;

private:
    RwLock* m_lock;
    RwLockType m_lockType;
};

} // namespace utils
} // namespace nx
