#ifndef QN_LONG_RUNNABLE_H
#define QN_LONG_RUNNABLE_H

#include <cassert>
#include <typeinfo>

#include <QtCore/QThread>
#include <QtCore/QSemaphore>

#include "warnings.h"

class QN_EXPORT QnLongRunnable: public QThread {
    Q_OBJECT
public:
    QnLongRunnable(): 
        m_needStop(false),
        m_onPause(false)
    {
        DEBUG_CODE(m_type = NULL);
    }

    virtual ~QnLongRunnable() {
        if(isRunning())
            qnCritical("Runnable instance was destroyed without a call to stop().");
    }

    virtual bool needToStop() const {
        return m_needStop;
    }

    virtual void pause() {
        m_semaphore.tryAcquire(m_semaphore.available());
        m_onPause = true;
    }

    virtual void resume() {
        m_onPause = false;
        m_semaphore.release();
    }

    void pauseDelay() {
        while(m_onPause && !needToStop())
            m_semaphore.tryAcquire(1, 50);
    }

    void smartSleep(int ms) {
        int n = ms / 100;

        for (int i = 0; i < n; ++i)
            if (!needToStop())
                msleep(100);

        if (!needToStop())
            msleep(ms % 100);
    }

    bool isPaused() const { return m_onPause; }

public slots:
    void start(Priority priority = InheritPriority) {
        if (isRunning()) // already runing;
            return;

        DEBUG_CODE(m_type = &typeid(*this));

        m_needStop = false;
        QThread::start(priority);
        assert(isRunning());
    }

    virtual void pleaseStop() {
        m_needStop = true;
        if (m_onPause)
            resume();
    }

    virtual void stop() {
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

protected:
    volatile bool m_needStop;
    volatile bool m_onPause;

    QSemaphore m_semaphore;

    DEBUG_CODE(const std::type_info *m_type;)
};


/**
 * Helper cleanup class to use QnLongRunnable inside Qt smart pointers in a 
 * non-blocking fashion.
 */
struct QnRunnableCleanup
{
    static inline void cleanup(QnLongRunnable *runnable)
    {
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
