// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

Q_MOC_INCLUDE("nx/vms/client/desktop/window_context.h")

namespace nx::vms::client::desktop {

class WindowContext;

class CallAlarmManager: public QObject
{
    Q_OBJECT
    Q_PROPERTY(nx::vms::client::desktop::WindowContext* context
        READ context WRITE setContext NOTIFY contextChanged)
    Q_PROPERTY(bool ringing READ isRinging NOTIFY ringingChanged)
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)

public:
    explicit CallAlarmManager(WindowContext* context, QObject* parent = nullptr);
    explicit CallAlarmManager(QObject* parent = nullptr);
    virtual ~CallAlarmManager() override;

    WindowContext* context() const;
    void setContext(WindowContext* value);

    bool isRinging() const;

    bool isActive() const;
    void setActive(bool value);

    static void registerQmlType();

signals:
    void contextChanged();
    void ringingChanged();
    void activeChanged();

private:
    struct  Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
