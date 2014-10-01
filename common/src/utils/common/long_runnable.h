#ifndef QN_LONG_RUNNABLE_H
#define QN_LONG_RUNNABLE_H

#include <common/config.h>

#include <QtCore/QThread>
#include <QtCore/QSharedPointer>

#include "singleton.h"
#include "semaphore.h"
#include "utils/common/stoppable.h"


class QnLongRunnablePoolPrivate;

class QN_EXPORT QnLongRunnable
:
    public QThread,
    public QnStoppable  //QnLongRunnable::pleaseStop moved to separate interface QnStoppable since not only threads need to be stopped
{
    Q_OBJECT

public:
    QnLongRunnable();
    virtual ~QnLongRunnable();

    bool needToStop() const;

    virtual void pause();
    virtual void resume();
    bool isPaused() const;
    void pauseDelay();

    void smartSleep(int ms);

    //!Returns thread id, corresponding to this object
    /*!
        This id is remembered in \a QnLongRunnable::initSystemThreadId
    */
    std::uintptr_t systemThreadId() const;

    //!Returns thread id of current thread. On unix uses \a gettid function instead of pthread_self. It allows to find thread in gdb
    static std::uintptr_t currentThreadSystemId();

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
    volatile bool m_needStop;
    volatile bool m_onPause;
    QnSemaphore m_semaphore;
    std::uintptr_t m_systemThreadId;
    QSharedPointer<QnLongRunnablePoolPrivate> m_pool;
    DEBUG_CODE(const std::type_info *m_type;)
};


class QnLongRunnablePool: public QObject, public Singleton<QnLongRunnablePool> {
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


/**
 * Helper cleanup class to use QnLongRunnable inside Qt smart pointers in a 
 * non-blocking fashion.
 */
struct QnRunnableCleanup {
    static inline void cleanup(QnLongRunnable *runnable) {
        if(!runnable)
            return;

        if(runnable->isRunning()) {
            QObject::connect(runnable, SIGNAL(finished()), runnable, SLOT(deleteLater()));
            runnable->pleaseStop();
        } else {
            delete runnable;
        }
    }
};


#endif // QN_LONG_RUNNABLE_H
