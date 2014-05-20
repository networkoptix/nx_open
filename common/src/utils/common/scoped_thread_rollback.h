#ifndef SCOPED_THREAD_ROLLBACK_H
#define SCOPED_THREAD_ROLLBACK_H

#include <QtCore/QThreadPool>


/*!
    Intended for use in thread of thread pool
*/
class QnScopedThreadRollback {
public:
    QnScopedThreadRollback(int increaseThreadCount):
        m_increaseThreadCount(increaseThreadCount)
    {
        //  Calling this function without previously reserving a thread temporarily increases maxThreadCount().
        for (int i = 0; i < m_increaseThreadCount; i++)
            QThreadPool::globalInstance()->releaseThread();
    }

    ~QnScopedThreadRollback() {
        for (int i = 0; i < m_increaseThreadCount; i++)
            QThreadPool::globalInstance()->reserveThread();
    }

private:
    int m_increaseThreadCount;

};

#define QN_INCREASE_MAX_THREADS(VALUE) QnScopedThreadRollback threadReserver(VALUE); Q_UNUSED(threadReserver);

#endif // SCOPED_THREAD_ROLLBACK_H
