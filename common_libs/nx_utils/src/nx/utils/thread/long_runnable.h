#ifndef QN_LONG_RUNNABLE_H
#define QN_LONG_RUNNABLE_H

#include <atomic>

#include <QtCore/QSharedPointer>
#include <QtCore/QThread>

#include <nx/utils/safe_direct_connection.h>
#include <nx/utils/singleton.h>

#include "semaphore.h"
#include "stoppable.h"
#include "../compiler_options.h"

class QnLongRunnablePoolPrivate;

class NX_UTILS_API QnLongRunnable:
    public QThread,
    public QnStoppable,  //QnLongRunnable::pleaseStop moved to separate interface QnStoppable since not only threads need to be stopped
    public Qn::EnableSafeDirectConnection
{
    Q_OBJECT

public:
    QnLongRunnable( bool isTrackedByPool = true );
    virtual ~QnLongRunnable();

    bool needToStop() const;

    virtual void pause();
    virtual void resume();
    virtual bool isPaused() const;
    void pauseDelay();

    void smartSleep(int ms);

    //!Returns thread id, corresponding to this object
    /*!
        This id is remembered in QnLongRunnable::initSystemThreadId
    */
    uintptr_t systemThreadId() const;

    //!Returns thread id of current thread. On unix uses gettid function instead of pthread_self. It allows to find thread in gdb
    static uintptr_t currentThreadSystemId();

public slots:
    virtual void start(Priority priority = InheritPriority);
    //!Implementation of QnStoppable::pleaseStop()
    virtual void pleaseStop() override;
    virtual void stop();

protected:
    void initSystemThreadId();

private slots:
    void at_started();
    void at_finished();

protected:
    std::atomic<bool> m_needStop;
    std::atomic<bool> m_onPause;
    QnSemaphore m_semaphore;
    uintptr_t m_systemThreadId;
    QSharedPointer<QnLongRunnablePoolPrivate> m_pool;
    DEBUG_CODE(const std::type_info *m_type;)
};


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

#endif // QN_LONG_RUNNABLE_H
