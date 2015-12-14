#ifndef QN_SEMAPHORE_H
#define QN_SEMAPHORE_H

#include <QtCore/qglobal.h>

class QnSemaphorePrivate;

// TODO: #AK or #VASILENKO add doxydocs: what is fixed here?
class QnSemaphore {
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

#endif // QnSemaphore_H
