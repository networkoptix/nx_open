// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

#include "joystick_device_info.h"
#include "osal_device.h"

namespace nx::vms::client::desktop::joystick {

class OsalDeviceListener: public QObject
{
public:
    OsalDeviceListener(QObject* parent = nullptr) : QObject(parent) {}

    virtual void onStateChanged(const OsalDevice::State& newState) = 0;
};

class OsalDriver: public QObject
{
    Q_OBJECT

public:
    OsalDriver() = default;
    virtual ~OsalDriver() override = default;

    static OsalDriver* getDriver();

    virtual QList<JoystickDeviceInfo> deviceList() = 0;
    virtual void setupDeviceListener(const QString& path, const OsalDeviceListener* listener) = 0;
    virtual void removeDeviceListener(const OsalDeviceListener* listener) = 0;

    virtual void halt() = 0;
    virtual void resume() = 0;

signals:
    void deviceListChanged();
};

} // namespace nx::vms::client::desktop::joystick
