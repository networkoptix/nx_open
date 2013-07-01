#ifndef QN_VARIANT_TIMER_H
#define QN_VARIANT_TIMER_H 

#include <boost/array.hpp>

#include <QAbstractEventDispatcher>
#include <QVariant>
#include <QTimer>

class QnVariantTimer: public QTimer {
    Q_OBJECT
    typedef QTimer base_type;

public:
    QnVariantTimer(QObject *parent = NULL): base_type(parent) {}

    const QVariant &arg(int n) {
        return m_args[n];
    }

    void setArg(int n, const QVariant &arg) {
        m_args[n] = arg;
    }

    void setArgs(const QVariant &arg0 = QVariant(), const QVariant &arg1 = QVariant(), const QVariant &arg2 = QVariant(), const QVariant &arg3 = QVariant(), const QVariant &arg4 = QVariant(), const QVariant &arg5 = QVariant(), const QVariant &arg6 = QVariant(), const QVariant &arg7 = QVariant(), const QVariant &arg8 = QVariant(), const QVariant &arg9 = QVariant()) {
        m_args[0] = arg0;
        m_args[1] = arg1;
        m_args[2] = arg2;
        m_args[3] = arg3;
        m_args[4] = arg4;
        m_args[5] = arg5;
        m_args[6] = arg6;
        m_args[7] = arg7;
        m_args[8] = arg8;
        m_args[9] = arg9;
    }

    using base_type::singleShot;

    static void singleShot(int msec, QObject *receiver, const char *member, const QVariant &arg0 = QVariant(), const QVariant &arg1 = QVariant(), const QVariant &arg2 = QVariant(), const QVariant &arg3 = QVariant(), const QVariant &arg4 = QVariant(), const QVariant &arg5 = QVariant(), const QVariant &arg6 = QVariant(), const QVariant &arg7 = QVariant(), const QVariant &arg8 = QVariant(), const QVariant &arg9 = QVariant()) {
        QnVariantTimer *timer = new QnVariantTimer(QAbstractEventDispatcher::instance());
        connect(timer, SIGNAL(timeout()), timer, SLOT(deleteLater()));
        connect(timer, SIGNAL(timeout(const QVariant &, const QVariant &, const QVariant &, const QVariant &, const QVariant &, const QVariant &, const QVariant &, const QVariant &, const QVariant &, const QVariant &)), receiver, member);
        timer->setArgs(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
        timer->setSingleShot(true);
        timer->setInterval(msec);
        timer->start();
    }

signals:
    void timeout(const QVariant &arg0, const QVariant &arg1, const QVariant &arg2, const QVariant &arg3, const QVariant &arg4, const QVariant &arg5, const QVariant &arg6, const QVariant &arg7, const QVariant &arg8, const QVariant &arg9);

protected:
    virtual void timerEvent(QTimerEvent *e) {
        int timerId = this->timerId();

        /* This call can change timer id. */
        base_type::timerEvent(e);

        if(e->timerId() == timerId)
            emit timeout(m_args[0], m_args[1], m_args[2], m_args[3], m_args[4], m_args[5], m_args[6], m_args[7], m_args[8], m_args[9]);
    }

private:
    boost::array<QVariant, 10> m_args;
};

#endif // QN_VARIANT_TIMER_H
