#ifndef QN_ANIMATOR_H
#define QN_ANIMATOR_H

#include <QObject>

#if 0
class QnAnimator: public QObject {
    Q_OBJECT;
public:
    QnAnimator(QObject *parent = NULL) {
        
    }

protected:
    

private:
    QObject *m_target;
    QnAbstractSetter *m_setter;
    QnAbstractGetter *m_getter;
    qreal m_speed;
    int m_defaultTimeoutMSec;
};
#endif


#endif // QN_ANIMATOR_H
