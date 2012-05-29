#ifndef cl_long_runnable_146
#define cl_long_runnable_146

#include <QThread>
#include <QSemaphore>
#include "warnings.h"

class QN_EXPORT CLLongRunnable : public QThread
{
    Q_OBJECT
signals:
    void threadPaused();
public slots:
    void start ( Priority priority = InheritPriority )
    {
        if (m_runing) // already runing;
            return;

        m_runing = true;
        m_needStop = false;
        QThread::start(priority);
    }

    virtual void pleaseStop()
    {
        m_needStop = true;
        if (m_onPause)
            resume();
    }

    virtual void stop()
    {
        pleaseStop();
        wait();
        m_runing = false;
    }

public:
    CLLongRunnable() : m_runing(false), m_onPause(false) {}
    
    virtual ~CLLongRunnable() 
    {
        if(m_runing)
            qnCritical("Runnable instance was destroyed without a call to stop().");
    }


    virtual bool needToStop() const
    {
        return m_needStop;
    }

    virtual void pause()
    {
        m_semaphore.tryAcquire(m_semaphore.available());
        m_onPause = true;

    }

    virtual void resume()
    {
        m_onPause = false;
        m_semaphore.release();
    }

    void pauseDelay()
    {
        if (m_onPause)
            emit threadPaused();
        while(m_onPause && !needToStop())
            m_semaphore.tryAcquire(1,50);
    }

    void smartSleep(int ms) // not precise 
    {
        int n = ms/100;
        
        for (int i = 0; i < n; ++i)
        {
            if (!needToStop())
                msleep(100);
        }

        if (!needToStop())
        {
            msleep(ms%100);
        }
    }
    bool onPause() const { return m_onPause; }
protected:

protected:
    bool m_runing;
    volatile bool m_needStop;

    volatile bool m_onPause;
    QSemaphore m_semaphore;
};


/**
 * Helper cleanup class to use CLLongRunnable inside Qt smart pointers in a 
 * non-blocking fashion.
 */
struct QnRunnableCleanup
{
    static inline void cleanup(CLLongRunnable *runnable)
    {
        if(runnable->isRunning()) {
            QObject::connect(runnable, SIGNAL(finished()), runnable, SLOT(deleteLater()));
            runnable->pleaseStop();
        } else {
            delete runnable;
        }
    }
};


#endif //cl_long_runnable_146
