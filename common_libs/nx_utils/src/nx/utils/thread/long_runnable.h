#pragma once

#include <atomic>

#include <QtCore/QSharedPointer>
#include <QtCore/QThread>

#include <nx/utils/safe_direct_connection.h>
#include <nx/utils/singleton.h>

#include "semaphore.h"
#include "stoppable.h"

namespace nx {
namespace utils {

class NX_UTILS_API Thread:
    public QThread,
    public QnStoppable,
    public Qn::EnableSafeDirectConnection
{
    Q_OBJECT

public:
    Thread();
    virtual ~Thread();

    bool needToStop() const;

    virtual void pause();
    virtual void resume();
    virtual bool isPaused() const;
    void pauseDelay();

    void smartSleep(int ms);

    /**
     * @return Thread id, corresponding to this object.
     * This id is remembered in Thread::initSystemThreadId.
     */
    uintptr_t systemThreadId() const;

    /**
     * @return Thread id of current thread. 
     * On unix uses gettid function instead of pthread_self.
     * It allows to find thread in gdb.
     */
    static uintptr_t currentThreadSystemId();

signals:
    void paused();

public slots:
    virtual void start(Priority priority = InheritPriority);
    virtual void pleaseStop() override;
    virtual void stop();

protected:
    void initSystemThreadId();

protected slots:
    virtual void at_started();
    virtual void at_finished();

protected:
    std::atomic<bool> m_needStop = false;
    std::atomic<bool> m_onPause = false;
    QnSemaphore m_semaphore;
    uintptr_t m_systemThreadId = 0;
    #if defined(_DEBUG)
        const std::type_info* m_type = nullptr;
    #endif
};

} // namespace utils
} // namespace nx

//-------------------------------------------------------------------------------------------------

class QnLongRunnablePoolPrivate;

class NX_UTILS_API QnLongRunnable:
    public nx::utils::Thread
{
    using base_type = nx::utils::Thread;

    Q_OBJECT

public:
    QnLongRunnable(bool isTrackedByPool = true);
    ~QnLongRunnable();

protected slots:
    virtual void at_started() override;
    virtual void at_finished() override;

private:
    QSharedPointer<QnLongRunnablePoolPrivate> m_pool;
};

//-------------------------------------------------------------------------------------------------

class NX_UTILS_API QnLongRunnablePool:
    public QObject,
    public Singleton<QnLongRunnablePool>
{
    Q_OBJECT

public:
    QnLongRunnablePool(QObject *parent = NULL);
    virtual ~QnLongRunnablePool();

    /**
     * Sends stop request to all threads previously registered with this pool
     * and waits until they are stopped.
     */
    void stopAll();

    /**
     * Blocks current thread until all threads previously registered with this
     * pool are stopped.
     */
    void waitAll();

private:
    friend class QnLongRunnable;
    QSharedPointer<QnLongRunnablePoolPrivate> d;
};
