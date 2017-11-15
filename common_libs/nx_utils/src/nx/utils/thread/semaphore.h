#ifndef QN_SEMAPHORE_H
#define QN_SEMAPHORE_H

#include <QtCore/qglobal.h>


class QnSemaphorePrivate;

/*!
    NOTE: Need own semaphore to add it to deadlock detection logic someday...
*/
class NX_UTILS_API QnSemaphore {
public:
    explicit QnSemaphore(int n = 0);
    ~QnSemaphore();

    void acquire(int n = 1);
    bool tryAcquire(int n = 1);
    /*!
        \param timeout Millis
    */
    bool tryAcquire(int n, int timeout);

    void release(int n = 1);

    int available() const;

private:
    Q_DISABLE_COPY(QnSemaphore)

    QnSemaphorePrivate *d;
};

template<typename Semaphore>
class QnGenericSemaphoreLocker
{
public:
    QnGenericSemaphoreLocker(Semaphore* semaphore, int resourceNumber = 1):
        m_resourceNumber(resourceNumber),
        m_semaphore(semaphore)
    {
        if (!m_semaphore)
            return;
        semaphore->acquire(m_resourceNumber);
    };

    virtual ~QnGenericSemaphoreLocker()
    {
        if (!m_semaphore)
            return;

        m_semaphore->release(m_resourceNumber);
    };

private:
    const int m_resourceNumber;
    Semaphore* m_semaphore;
};

using QnSemaphoreLocker = QnGenericSemaphoreLocker<QnSemaphore>;

#endif // QnSemaphore_H
