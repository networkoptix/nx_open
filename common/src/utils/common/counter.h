#ifndef QN_COUNTER_H
#define QN_COUNTER_H

#include <QObject>

class QnCounter: public QObject {
    Q_OBJECT;
public:
    QnCounter(int targetCount, QObject *parent = NULL):    
        QObject(parent),
        m_count(0), 
        m_targetCount(targetCount)
    {}

signals:
    void targetReached();

public slots:
    void increment() {
        m_count++;
        if(m_count == m_targetCount)
            emit targetReached();
    }

private:
    int m_count;
    int m_targetCount;
};

#endif // QN_COUNTER_H