#ifndef QN_PLATFORM_NOTIFIER_H
#define QN_PLATFORM_NOTIFIER_H

#include <QtCore/QObject>

class QnPlatformNotifier: public QObject {
    Q_OBJECT;
public:
    QnPlatformNotifier(QObject *parent = NULL): QObject(parent) {}
    virtual ~QnPlatformNotifier() {}

    static QnPlatformNotifier *newInstance(QObject *parent = NULL);

signals:
    /**
     * This signal is emitted whenever system time changes. 
     */
    void timeChanged();

    /**
     * This signal is emitted whenever system time zone changes.
     */
    void timeZoneChanged();

private:
    Q_DISABLE_COPY(QnPlatformNotifier)
};


#endif // QN_PLATFORM_NOTIFIER_H
