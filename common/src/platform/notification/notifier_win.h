#ifndef QN_WINDOWS_NOTIFIER_H
#define QN_WINDOWS_NOTIFIER_H

#include "platform_notifier.h"

class QnWindowsNotifier: public QnPlatformNotifier {
    Q_OBJECT;

    typedef QnPlatformNotifier base_type;

public:
    QnWindowsNotifier(QObject *parent = NULL);
    virtual ~QnWindowsNotifier();

    virtual bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateTime(bool notify);

private:
    qint64 m_timeZoneOffset;
};

#endif // QN_WINDOWS_NOTIFIER_H
