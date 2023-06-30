// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QString>

#include "os_hid_device.h"

namespace nx::vms::client::desktop::joystick {

class OsHidDeviceSubscriber: public QObject
{
public:
    OsHidDeviceSubscriber(QObject* parent = nullptr) : QObject(parent) {}
    virtual void onStateChanged(const QBitArray& newState) = 0;
};

class OsHidDriver: public QObject
{
    Q_OBJECT

public:
    OsHidDriver() = default;
    virtual ~OsHidDriver() override = default;

    static OsHidDriver* getDriver();

    virtual QList<OsHidDeviceInfo> deviceList() = 0;
    virtual void setupDeviceSubscriber(const QString& path,
        const OsHidDeviceSubscriber* subscriber) = 0;
    virtual void removeDeviceSubscriber(const OsHidDeviceSubscriber* subscriber) = 0;

signals:
    void deviceListChanged();
};

} // namespace nx::vms::client::desktop::joystick
