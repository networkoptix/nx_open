// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_WINDOWS_NOTIFIER_H
#define QN_WINDOWS_NOTIFIER_H

#include "platform_notifier.h"

class QnWindowsNotifier: public QnPlatformNotifier {
    Q_OBJECT
    typedef QnPlatformNotifier base_type;

public:
    enum Value {
        CompositionValue
    };

    QnWindowsNotifier(QObject *parent = nullptr);
    virtual ~QnWindowsNotifier();

    virtual bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void compositionChanged();

protected:
    virtual void notify(int value) override;

private:
    void updateTime(bool notify);

private:
    qint64 m_timeZoneOffset;
};

#endif // QN_WINDOWS_NOTIFIER_H
