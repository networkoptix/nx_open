// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "semaphore.h"

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/log/assert.h>
#include <QtCore/QElapsedTimer>
#include <QtCore/QDateTime>

namespace nx {

namespace detail {

class SemaphoreImpl
{
public:
    explicit SemaphoreImpl(int count): count(count) {}

    nx::Mutex mutex;
    nx::WaitCondition waitCondition;
    int count;
};

} // namespace detail

Semaphore::Semaphore(int count /*= 0*/)
{
    if (!NX_ASSERT(count >= 0))
        count = 0;

    d = new nx::detail::SemaphoreImpl(count);
}

Semaphore::~Semaphore()
{
    delete d;
}

void Semaphore::acquire(int count)
{
    if (!NX_ASSERT(count >= 0))
        return;

    NX_MUTEX_LOCKER lock(&d->mutex);

    while (count > d->count)
        d->waitCondition.wait(lock.mutex());
    d->count -= count;
}

void Semaphore::release(int count)
{
    if (!NX_ASSERT(count >= 0))
        return;

    NX_MUTEX_LOCKER lock(&d->mutex);

    d->count += count;
    d->waitCondition.wakeAll();
}

int Semaphore::available() const
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    return d->count;
}

bool Semaphore::tryAcquire(int count)
{
    if (!NX_ASSERT(count >= 0))
        return false;

    NX_MUTEX_LOCKER lock(&d->mutex);

    if (count > d->count)
        return false;
    d->count -= count;
    return true;
}

bool Semaphore::tryAcquire(int count, int timeoutMs)
{
    if (!NX_ASSERT(count >= 0))
        return false;

    NX_MUTEX_LOCKER lock(&d->mutex);

    if (timeoutMs < 0)
    {
        while (count > d->count)
            d->waitCondition.wait(lock.mutex());
    }
    else
    {
        QElapsedTimer timer;
        timer.start();
        while (count > d->count)
        {
            const qint64 elapsedMs = timer.elapsed();
            if (elapsedMs > timeoutMs
                || !d->waitCondition.wait(lock.mutex(), timeoutMs - elapsedMs))
            {
                return false;
            }
        }
    }

    d->count -= count;
    return true;
}

} // namespace nx
