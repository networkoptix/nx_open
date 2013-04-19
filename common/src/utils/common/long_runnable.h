#ifndef QN_LONG_RUNNABLE_H
#define QN_LONG_RUNNABLE_H

#include "config.h" 

#include <QtCore/QThread>
#include <QtCore/QSharedPointer>

#include "singleton.h"
#include "qnsemaphore.h"

class QnLongRunnablePoolPrivate;


class QN_EXPORT QnLongRunnable: public QThread {
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

    int sysThreadID() const;

public slots:
    virtual void start(Priority priority = InheritPriority);
    virtual void pleaseStop();
    virtual void stop();

private slots:
    void at_started();
    void at_finished();

protected:
    volatile bool m_needStop;
    volatile bool m_onPause;
    QnSemaphore m_semaphore;
    int m_sysThreadID;
    QSharedPointer<QnLongRunnablePoolPrivate> m_pool;
    DEBUG_CODE(const std::type_info *m_type;)

    void saveSysThreadID();
};


class QnLongRunnablePool: public QObject, public QnSingleton<QnLongRunnablePool> {
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
