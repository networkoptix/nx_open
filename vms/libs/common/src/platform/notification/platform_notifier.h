#ifndef QN_PLATFORM_NOTIFIER_H
#define QN_PLATFORM_NOTIFIER_H

#include <QtCore/QObject>

class QnPlatformNotifier: public QObject {
    Q_OBJECT;
public:
    enum Value {
        TimeValue,
        TimeZoneValue,
        PlatformValue = 0x100
    };

    QnPlatformNotifier(QObject *parent = NULL): QObject(parent) {}
    virtual ~QnPlatformNotifier() {}

signals:
    /**
     * \param value                     Platform value that has changed.
     */
    void changed(int value);

    /**
     * This signal is emitted whenever system time changes. 
     */
    void timeChanged();

    /**
     * This signal is emitted whenever system time zone changes.
     */
    void timeZoneChanged();

protected:
    virtual void notify(int value) {
        emit changed(value);
        if(value == TimeValue)
            emit timeChanged();
        if(value == TimeZoneValue)
            emit timeZoneChanged();
    }

private:
    Q_DISABLE_COPY(QnPlatformNotifier)
};


#endif // QN_PLATFORM_NOTIFIER_H
