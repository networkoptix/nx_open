// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QBitArray>
#include <QtCore/QObject>

#include <nx/vms/client/desktop/joystick/settings/osal/osal_driver.h>

namespace nx::vms::client::desktop::joystick {

class JoystickManager;

class JoystickDevice: public OsalDeviceListener
{
    Q_OBJECT

public:
    virtual ~JoystickDevice() override;

    Q_INVOKABLE QString id() const { return m_deviceInfo.id; }
    Q_INVOKABLE QString path() const { return m_deviceInfo.path; }
    Q_INVOKABLE QString modelName() const { return m_deviceInfo.modelName; }
    Q_INVOKABLE QString manufacturerName() const { return m_deviceInfo.manufacturerName; }

public:
    virtual void onStateChanged(const OsalDevice::State& newState) override;

signals:
    void stateChanged(const QList<bool>& state); //< This signal is for using in QML.

protected:
    JoystickDevice(JoystickDeviceInfo deviceInfo, QObject* parent);

    JoystickDeviceInfo m_deviceInfo;

    friend class JoystickManager;
};

} // namespace nx::vms::client::desktop::joystick
