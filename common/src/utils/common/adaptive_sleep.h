#ifndef cl_adaptive_sleep_137
#define cl_adaptive_sleep_137

#include <utils/common/mutex.h>
#include <utils/common/wait_condition.h>
#include <QtCore/QTime>
#include "sleep.h"

#define MAX_VALID_SLEEP_TIME 5000000

class QnAdaptiveSleep {
public:
    /**
     * Constructor.
     * 
     * The only parameter defines the maximal difference between expected and
     * actual time slept, in microseconds. When this threshold is reached,
     * timer (the 'expected' time slept) is readjusted to be in line with the
     * actual time slept.
     * 
     * Pass zero to disable the adjustment.
     * 
     * \param maxOverdraftUSec          Maximal difference between expected and
     *                                  actual time slept, in microseconds. 
     */
    QnAdaptiveSleep(int maxOverdraftUSec):
        m_firstTime(true),
        m_maxOverdraft(maxOverdraftUSec)
    {};

    void setMaxOverdraft(int maxOverdraftUSec) 
    {
        m_maxOverdraft = maxOverdraftUSec;
        afterdelay();
    }

    int sleep(qint64 usec, qint64 maxSleepTime = INT_MAX)
    {
        if (m_firstTime)
        {
            m_firstTime = false;
            m_prevEndTime.start();
            m_totalTime = 0;
        }

        m_totalTime += usec;

        qint64 now = (qint64)m_prevEndTime.elapsed() * 1000;
        qint64 havetowait = m_totalTime - now;

        if (havetowait<=0)
        {
            if (-havetowait > m_maxOverdraft && m_maxOverdraft > 0)
                afterdelay();
            return havetowait;
        }

        if (havetowait < MAX_VALID_SLEEP_TIME)
            QnSleep::msleep(qMin(havetowait / 1000, maxSleepTime / 1000));
        else
            afterdelay();
        return havetowait;
    }

    int terminatedSleep(qint64 usec, qint64 maxSleepTime = INT_MAX)
    {
        if (m_firstTime)
        {
            m_firstTime = false;
            m_prevEndTime.start();
            m_totalTime = 0;
        }

        m_totalTime += usec;

        qint64 now = (qint64)m_prevEndTime.elapsed() * 1000;
        qint64 havetowait = m_totalTime - now;

        if (havetowait<=0)
        {
            if (-havetowait > m_maxOverdraft && m_maxOverdraft > 0)
                afterdelay();
            return havetowait;
        }
        //NX_LOG("sleep time=", havetowait/1000000.0, cl_logALWAYS);
        if (havetowait < MAX_VALID_SLEEP_TIME) 
        {
            SCOPED_MUTEX_LOCK( lock, &m_mutex);
            m_waitCond.wait(&m_mutex, qMin(havetowait / 1000, maxSleepTime / 1000));
        }
        else {
            afterdelay();
        }
        return havetowait;
    }

    void breakSleep()
    {
        m_waitCond.wakeAll();
    }

    int addQuant(qint64 usec)
    {
        if (m_firstTime)
        {
            m_firstTime = false;
            m_prevEndTime.start();
            m_totalTime = 0;
        }
        m_totalTime += usec;
        qint64 now = (qint64) m_prevEndTime.elapsed() * 1000;
        qint64 havetowait = m_totalTime - now;
        return havetowait;
    }

    void afterdelay()
    {
        m_firstTime = true;
    }

private:
    QTime m_prevEndTime;
    bool m_firstTime;
    qint64 m_maxOverdraft;
    qint64 m_totalTime;
    
    QnMutex m_mutex;
    QnWaitCondition m_waitCond;
};

#endif //cl_adaptive_sleep_137
