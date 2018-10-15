#include "long_runnable.h"

#include <nx/utils/crash_dump/systemexcept.h>
#include <nx/utils/log/log.h>

#include "thread_util.h"

// -------------------------------------------------------------------------- //
// QnLongRunnablePoolPrivate
// -------------------------------------------------------------------------- //
class QnLongRunnablePoolPrivate {
public:
    QnLongRunnablePoolPrivate() {}

    void stopAll() {
        QnMutexLocker locker( &m_mutex );
        for(QnLongRunnable *runnable: m_created)
            runnable->pleaseStop();
        waitAllLocked();
    }

    void waitAll() {
        QnMutexLocker locker( &m_mutex );
        waitAllLocked();
    }

    void createdNotify(QnLongRunnable *runnable) {
        QnMutexLocker locker( &m_mutex );

        NX_ASSERT(runnable && !m_created.contains(runnable));

        m_created.insert(runnable);
    }

    void startedNotify(QnLongRunnable *runnable) {
        QnMutexLocker locker( &m_mutex );

        NX_ASSERT(runnable && !m_running.contains(runnable));

        m_running.insert(runnable);
    }

    void finishedNotify(QnLongRunnable *runnable) {
        QnMutexLocker locker( &m_mutex );

        NX_ASSERT(runnable); //  && m_running.contains(runnable)

        m_running.remove(runnable);
        if(m_running.isEmpty())
            m_waitCondition.wakeAll();
    }

    void destroyedNotify(QnLongRunnable *runnable) {
        QnMutexLocker locker( &m_mutex );

        NX_ASSERT(runnable && m_created.contains(runnable));

        m_created.remove(runnable);
    }

private:
    void waitAllLocked() {
        while(!m_running.isEmpty())
            m_waitCondition.wait(&m_mutex);
    }

private:
    QnMutex m_mutex;
    QnWaitCondition m_waitCondition;
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

// -------------------------------------------------------------------------- //
// QnLongRunnable
// -------------------------------------------------------------------------- //
QnLongRunnable::QnLongRunnable(bool isTrackedByPool)
{
    if (isTrackedByPool)
    {
        if (QnLongRunnablePool* pool = QnLongRunnablePool::instance())
        {
            m_pool = pool->d;
            m_pool->createdNotify(this);
        }
        else
        {
            NX_INFO(this, "QnLongRunnablePool instance does not exist, "
                "lifetime of this runnable will not be tracked.");
        }
    }

    connect(this, SIGNAL(started()), this, SLOT(at_started()), Qt::DirectConnection);
    connect(this, SIGNAL(finished()), this, SLOT(at_finished()), Qt::DirectConnection);

    #if !defined(Q_OS_ANDROID) //< Not supported on Android.
        // Thread stack size reduced (from default 8MB on Linux) to save memory on edge devices.
        // If this is not enough consider using heap.

        // This value is not platform-dependent to be able to catch stack overflow error on every
        // platform.

        constexpr size_t kDefaultThreadStackSize = 128 * 1024;
        setStackSize(kDefaultThreadStackSize);
    #endif
}

QnLongRunnable::~QnLongRunnable() {
    if (isRunning())
    {
        NX_ASSERT(false, "Runnable instance was destroyed without a call to stop().");
    }

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

bool QnLongRunnable::isPaused() const 
{
    return m_onPause;
}

void QnLongRunnable::pauseDelay()
{
    while(m_onPause && !needToStop())
    {
        emit paused();
        m_semaphore.tryAcquire(1, 50);
    }
}

uintptr_t QnLongRunnable::systemThreadId() const {
    return m_systemThreadId;
}

void QnLongRunnable::initSystemThreadId() {
    if(m_systemThreadId)
        return;

    m_systemThreadId = currentThreadSystemId();
}

uintptr_t QnLongRunnable::currentThreadSystemId()
{
    return ::currentThreadSystemId();
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

    #if defined(_DEBUG)
        m_type = &typeid(*this);
    #endif

    m_needStop = false;
    QThread::start(priority);
}

void QnLongRunnable::pleaseStop() {
    m_needStop = true;
    if (m_onPause)
        resume();
}

void QnLongRunnable::stop()
{
    #if defined(_DEBUG)
        if (m_type)
        {
            NX_ASSERT(typeid(*this) == *m_type,
                "stop() must be called from derived class destructor.");
            m_type = nullptr; //< So that we don't check it again.
        }
    #endif

    pleaseStop();
    wait();
}

void QnLongRunnable::at_started() {
    initSystemThreadId();

#ifdef _WIN32
    win32_exception::installThreadSpecificUnhandledExceptionHandler();
#endif

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    linux_exception::installQuitThreadBacktracer();
#endif

    if(m_pool)
        m_pool->startedNotify(this);
}

void QnLongRunnable::at_finished() {
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    linux_exception::uninstallQuitThreadBacktracer();
#endif

    if(m_pool)
        m_pool->finishedNotify(this);
}
