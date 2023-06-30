// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariantMap>

#include <nx/utils/impl_ptr.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop::joystick {

class JoystickDevice;

class JoystickManager: public QObject
{
    Q_OBJECT

public:
    JoystickManager();
    virtual ~JoystickManager() override;

    Q_INVOKABLE QVariantMap devices() const;
    Q_INVOKABLE nx::vms::client::desktop::joystick::JoystickDevice* createDevice(
        const QString& path);

public slots:
    void updateHidDeviceList();

    static void registerQmlType();

signals:
    void deviceListChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::joystick
