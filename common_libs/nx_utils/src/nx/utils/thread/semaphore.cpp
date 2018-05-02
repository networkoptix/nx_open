#include "semaphore.h"

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/log/assert.h>
#include <QtCore/QElapsedTimer>
#include <QtCore/QDateTime>


class QnSemaphorePrivate {
public:
    inline QnSemaphorePrivate(int n) : avail(n) { }

    QnMutex mutex;
    QnWaitCondition cond;

    int avail;
};

/*!
    Creates a new semaphore and initializes the number of resources
    it guards to n (by default, 0).

    \sa release(), available()
*/

QnSemaphore::QnSemaphore(int n)
{
    NX_ASSERT(n >= 0, "QnSemaphore", "parameter 'n' must be non-negative");
    d = new QnSemaphorePrivate(n);
}

/*!
    Destroys the semaphore.

    \warning Destroying a semaphore that is in use may result in
    undefined behavior.
*/
QnSemaphore::~QnSemaphore()
{ delete d; }

/*!
    Tries to acquire \c n resources guarded by the semaphore. If n
    > available(), this call will block until enough resources are
    available.

    \sa release(), available(), tryAcquire()
*/
void QnSemaphore::acquire(int n)
{
    NX_ASSERT(n >= 0, "QnSemaphore::acquire", "parameter 'n' must be non-negative");
    QnMutexLocker locker( &d->mutex );
    while (n > d->avail)
        d->cond.wait(locker.mutex());
    d->avail -= n;
}

/*!
    Releases n resources guarded by the semaphore.

    This function can be used to "create" resources as well. For
    example:

    \snippet doc/src/snippets/code/src_corelib_thread_QnSemaphore.cpp 1

    \sa acquire(), available()
*/
void QnSemaphore::release(int n)
{
    NX_ASSERT(n >= 0, "QnSemaphore::release", "parameter 'n' must be non-negative");
    QnMutexLocker locker( &d->mutex );
    d->avail += n;
    d->cond.wakeAll();
}

/*!
    Returns the number of resources currently available to the
    semaphore. This number can never be negative.

    \sa acquire(), release()
*/
int QnSemaphore::available() const
{
    QnMutexLocker locker( &d->mutex );
    return d->avail;
}

/*!
    Tries to acquire \c n resources guarded by the semaphore and
    returns true on success. If available() < n, this call
    immediately returns false without acquiring any resources.

    Example:

    \snippet doc/src/snippets/code/src_corelib_thread_QnSemaphore.cpp 2

    \sa acquire()
*/
bool QnSemaphore::tryAcquire(int n)
{
    NX_ASSERT(n >= 0, "QnSemaphore::tryAcquire", "parameter 'n' must be non-negative");
    QnMutexLocker locker( &d->mutex );
    if (n > d->avail)
        return false;
    d->avail -= n;
    return true;
}

/*!
    Tries to acquire \c n resources guarded by the semaphore and
    returns true on success. If available() < n, this call will
    wait for at most timeout milliseconds for resources to become
    available.

    Note: Passing a negative number as the timeout is equivalent to
    calling acquire(), i.e. this function will wait forever for
    resources to become available if timeout is negative.

    Example:

    \snippet doc/src/snippets/code/src_corelib_thread_QnSemaphore.cpp 3

    \sa acquire()
*/
bool QnSemaphore::tryAcquire(int n, int timeout)
{
    NX_ASSERT(n >= 0, "QnSemaphore::tryAcquire", "parameter 'n' must be non-negative");
    QnMutexLocker locker( &d->mutex );
    if (timeout < 0) {
        while (n > d->avail)
            d->cond.wait(locker.mutex());
    } else {
        QElapsedTimer timer;
        timer.start();
        while (n > d->avail) {
            int elapsed = timer.elapsed();
            if (elapsed > timeout
                || !d->cond.wait(locker.mutex(), timeout - elapsed))
                return false;
        }
    }
    d->avail -= n;
    return true;


}

