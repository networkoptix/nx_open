#ifndef QN_GENERIC_NOTIFIER_H
#define QN_GENERIC_NOTIFIER_H

#include "platform_notifier.h"

#include <QtCore/QBasicTimer>
#include <QtCore/QElapsedTimer>

class QnGenericNotifier: public QnPlatformNotifier {
    Q_OBJECT
    typedef QnPlatformNotifier base_type;

public:
    QnGenericNotifier(QObject *parent = NULL);
    virtual ~QnGenericNotifier();

protected:
    virtual void timerEvent(QTimerEvent *event) override;

private:
    void updateTimes(bool notify);

private:
    QElapsedTimer m_elapsedTimer;
    QBasicTimer m_checkTimer, m_restartTimer;

    qint64 m_lastUtcTime;
    qint64 m_lastLocalTime;
    qint64 m_lastElapsedTime;
    qint64 m_lastTimeZoneOffset;
};


#endif // QN_GENERIC_NOTIFIER_H
