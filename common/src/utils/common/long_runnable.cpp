#include "long_runnable.h"

#include <cassert>
#include <typeinfo>

#include <QtCore/QSet>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>

#include <common/systemexcept_win32.h>

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#   include <sys/types.h>
#   include <linux/unistd.h>
static pid_t gettid(void) { return syscall(__NR_gettid); }
#endif


// -------------------------------------------------------------------------- //
// QnLongRunnablePoolPrivate
// -------------------------------------------------------------------------- //
class QnLongRunnablePoolPrivate {
public:
    QnLongRunnablePoolPrivate() {}

    void stopAll() {
        QMutexLocker locker(&m_mutex);
        for(QnLongRunnable *runnable: m_created)
            runnable->pleaseStop();
        waitAllLocked();
    }

    void waitAll() {
        QMutexLocker locker(&m_mutex);
        waitAllLocked();
    }

    void createdNotify(QnLongRunnable *runnable) {
        QMutexLocker locker(&m_mutex);

        assert(runnable && !m_created.contains(runnable));

        m_created.insert(runnable);
    }

    void startedNotify(QnLongRunnable *runnable) {
        QMutexLocker locker(&m_mutex);

        assert(runnable && !m_running.contains(runnable));

        m_running.insert(runnable);
    }

    void finishedNotify(QnLongRunnable *runnable) {
        QMutexLocker locker(&m_mutex);

        assert(runnable); //  && m_running.contains(runnable)

        m_running.remove(runnable);
        if(m_running.isEmpty())
            m_waitCondition.wakeAll();
    }

    void destroyedNotify(QnLongRunnable *runnable) {
        QMutexLocker locker(&m_mutex);

        assert(runnable && m_created.contains(runnable));

        m_created.remove(runnable);
    }

private:
    void waitAllLocked() {
        while(!m_running.isEmpty())
            m_waitCondition.wait(&m_mutex);
    }

private:
    QMutex m_mutex;
    QWaitCondition m_waitCondition;
    QSet<QnLongRunnable *> m_created, m_running;
};


// -------------------------------------------------------------------------- //
// QnLongRunnablePool
// -------------------------------------------------------------------------- //
QnLongRunnablePool::QnLongRunnablePool(QObject *parent): 
    QObject(parent),
    d(new QnLongRunnablePoolPrivate())
{}

QnLongRunnablePool::~QnLongRunnablePool() {
    stopAll();
}

void QnLongRunnablePool::stopAll() {
    d->stopAll();
}

void QnLongRunnablePool::waitAll() {
    d->waitAll();
}


//!Thread stack size reduced (from default 8MB on linux) to save memory on edge devices. If this is not enough consider using heap
/*!
    This value is not platform-dependent to be able to catch stack overflow error on every platform
*/
static const size_t DEFAULT_THREAD_STACK_SIZE = 128*1024;

// -------------------------------------------------------------------------- //
// QnLongRunnable
// -------------------------------------------------------------------------- //
QnLongRunnable::QnLongRunnable(): 
    m_needStop(false),
    m_onPause(false),
    m_systemThreadId(0)
{
    DEBUG_CODE(m_type = NULL);

    if(QnLongRunnablePool *pool = QnLongRunnablePool::instance()) {
        m_pool = pool->d;
        m_pool->createdNotify(this);
    } else {
        qnWarning("QnLongRunnablePool instance does not exist, lifetime of this runnable will not be tracked.");
    }

    connect(this, SIGNAL(started()),    this, SLOT(at_started()), Qt::DirectConnection);
    connect(this, SIGNAL(finished()),   this, SLOT(at_finished()), Qt::DirectConnection);

#ifndef Q_OS_ANDROID // not supported on Android
    setStackSize( DEFAULT_THREAD_STACK_SIZE );
#endif
}

QnLongRunnable::~QnLongRunnable() {
    if(isRunning())
        qnCritical("Runnable instance was destroyed without a call to stop().");

    if(m_pool)
        m_pool->destroyedNotify(this);
}

bool QnLongRunnable::needToStop() const {
    return m_needStop;
}

void QnLongRunnable::pause() {
    m_semaphore.tryAcquire(m_semaphore.available());
    m_onPause = true;
}

void QnLongRunnable::resume() {
    m_onPause = false;
    m_semaphore.release();
}

bool QnLongRunnable::isPaused() const { 
    return m_onPause; 
}

void QnLongRunnable::pauseDelay() {
    while(m_onPause && !needToStop())
        m_semaphore.tryAcquire(1, 50);
}

std::uintptr_t QnLongRunnable::systemThreadId() const {
    return m_systemThreadId;
}

void QnLongRunnable::initSystemThreadId() {
    if(m_systemThreadId)
        return;

    m_systemThreadId = currentThreadSystemId();
}

std::uintptr_t QnLongRunnable::currentThreadSystemId()
{
#if defined(Q_OS_LINUX)
    /* This one is purely for debugging purposes.
    * QThread::currentThreadId is implemented via pthread_self,
    * which is not an identifier you see in GDB. */
    return gettid();
#else
    return reinterpret_cast<std::uintptr_t>(QThread::currentThreadId());
#endif
}

void QnLongRunnable::smartSleep(int ms) {
    int n = ms / 100;

    for (int i = 0; i < n; ++i)
        if (!needToStop())
            msleep(100);

    if (!needToStop())
        msleep(ms % 100);
}

void QnLongRunnable::start(Priority priority) {
    if (isRunning())
        return;

    DEBUG_CODE(m_type = &typeid(*this));

    m_needStop = false;
    QThread::start(priority);
}

void QnLongRunnable::pleaseStop() {
    m_needStop = true;
    if (m_onPause)
        resume();
}

void QnLongRunnable::stop() {
    DEBUG_CODE(
        if(m_type) {
            const std::type_info *type = &typeid(*this);
            assert(*type == *m_type); /* You didn't call stop() from derived class's destructor! Die! */

            m_type = NULL; /* So that we don't check it again. */
        }
    );

    pleaseStop();
    wait();
}

void QnLongRunnable::at_started() {
#ifdef _WIN32
    win32_exception::installThreadSpecificUnhandledExceptionHandler();
#endif

    if(m_pool)
        m_pool->startedNotify(this);
}

void QnLongRunnable::at_finished() {
    if(m_pool)
        m_pool->finishedNotify(this);
}
