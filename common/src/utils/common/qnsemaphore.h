#ifndef QN_QnSemaphore_H
#define QN_QnSemaphore_H

#include <QtCore/qglobal.h>

class QnSemaphorePrivate;

// TODO: #Elric rename file
class QnSemaphore
{
public:
    explicit QnSemaphore(int n = 0);
    ~QnSemaphore();

    void acquire(int n = 1);
    bool tryAcquire(int n = 1);
    bool tryAcquire(int n, int timeout);

    void release(int n = 1);

    int available() const;

private:
    Q_DISABLE_COPY(QnSemaphore)

    QnSemaphorePrivate *d;
};

#endif // QnSemaphore_H
