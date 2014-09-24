#ifndef SCOPED_THREAD_ROLLBACK_H
#define SCOPED_THREAD_ROLLBACK_H

#include <QtCore/QThreadPool>
#include <QtCore/QPointer>

/*!
    Intended for use in thread of thread pool
*/
class QnScopedThreadRollback {
public:
    QnScopedThreadRollback(int increaseThreadCount, QThreadPool* threadPool = nullptr):
        m_increaseThreadCount(increaseThreadCount),
        m_threadPool(threadPool)
    {
        //  Calling this function without previously reserving a thread temporarily increases maxThreadCount().
        for (int i = 0; i < m_increaseThreadCount; i++)
            (m_threadPool ? m_threadPool.data() : QThreadPool::globalInstance())->releaseThread();
    }

    ~QnScopedThreadRollback() {
        for (int i = 0; i < m_increaseThreadCount; i++)
            (m_threadPool ? m_threadPool.data() : QThreadPool::globalInstance())->reserveThread();
    }

private:
    int m_increaseThreadCount;
    QPointer<QThreadPool> m_threadPool;

};

#define QN_INCREASE_MAX_THREADS(VALUE) QnScopedThreadRollback threadReserver(VALUE); Q_UNUSED(threadReserver);

#endif // SCOPED_THREAD_ROLLBACK_H
