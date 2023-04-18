// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

#include "os_hid_device.h"

namespace nx::vms::client::desktop::joystick {

class OsHidDriver: public QObject
{
    Q_OBJECT

public:
    OsHidDriver() = default;
    virtual ~OsHidDriver() override = default;

    static OsHidDriver* getDriver();

    virtual QList<OsHidDeviceInfo> deviceList() = 0;
    virtual void setupDeviceSubscriber(const QString& path, QObject* subscriber) = 0;
    virtual void removeDeviceSubscriber(QObject* subscriber) = 0;

signals:
    void deviceListChanged();
};

} // namespace nx::vms::client::desktop::joystick
