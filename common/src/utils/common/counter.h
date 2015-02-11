#ifndef QN_COUNTER_H
#define QN_COUNTER_H

#include <QtCore/QObject>
#include <utils/common/mutex.h>

class QnCounter: public QObject {
    Q_OBJECT;
public:
    QnCounter(int initialCount, QObject *parent = NULL):    
        QObject(parent),
        m_count(initialCount)
    {}

signals:
    void reachedZero();

public slots:
    void increment() {
        SCOPED_MUTEX_LOCK( locker, &m_mutex);
        m_count++;
    }

    void decrement() {
        SCOPED_MUTEX_LOCK( locker, &m_mutex);
        m_count--;
        if(m_count == 0)
            emit reachedZero();
    }

private:
    QnMutex m_mutex;
    int m_count;
};

#endif // QN_COUNTER_H
