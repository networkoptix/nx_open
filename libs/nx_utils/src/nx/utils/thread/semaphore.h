// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/qglobal.h>

namespace nx {

namespace detail { class SemaphoreImpl; }

/**
 * Mimics behavior of QSemaphore. The intention of creating a dedicated class is to potentially
 * support deadlock detection logic.
 */
class NX_UTILS_API Semaphore
{
public:
    /**
     * Creates a new semaphore and initializes the number of resources it guards to `count`.
     */
    explicit Semaphore(int count = 0);

    /**
     * NOTE: Destroying a semaphore that is in use may result in undefined behavior.
     */
    ~Semaphore();

    /**
     * Attempts acquiring `count` resources guarded by the semaphore. If `count` > available(),
     * this call will block until enough resources are available.
     */
    void acquire(int count = 1);

    /**
     * Attempts acquiring `count` resources guarded by the semaphore and returns true on success.
     * If available() < count, this call immediately returns false without acquiring any resources.
     */
    bool tryAcquire(int count = 1);

    /**
     * Attempts acquiring `count` resources guarded by the semaphore and returns true on success.
     * If available() < count, this call will wait for at most timeoutMs milliseconds for resources
     * to become available.
     *
     * NOTE: Negative timeoutMs means indefinitely, so the call becomes equivalent to acquire().
     */
    bool tryAcquire(int count, int timeoutMs);

    /**
     * Releases `count` resources guarded by the semaphore. Can also be used to "create" resources.
     */
    void release(int count = 1);

    /**
     * @return The number of resources currently available to the semaphore; never negative.
     */
    int available() const;

private:
    Q_DISABLE_COPY(Semaphore)

    nx::detail::SemaphoreImpl* d;
};

class SemaphoreLocker
{
public:
    SemaphoreLocker(Semaphore* semaphore): m_semaphore(semaphore) { m_semaphore->acquire(1); }
    ~SemaphoreLocker() { m_semaphore->release(1); }

private:
    Semaphore* m_semaphore;
};

} // namespace nx
