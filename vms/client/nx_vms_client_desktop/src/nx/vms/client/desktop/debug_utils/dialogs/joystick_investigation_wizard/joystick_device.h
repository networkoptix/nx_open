// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QBitArray>
#include <QtCore/QObject>

#include <nx/vms/client/desktop/joystick/settings/osal/osal_driver.h>

namespace nx::vms::client::desktop::joystick {

class JoystickManager;

class AxisDescrition
{
    Q_GADGET

    Q_PROPERTY(bool isInitialized READ getIsInitialized CONSTANT)
    Q_PROPERTY(int min READ getMin CONSTANT)
    Q_PROPERTY(int max READ getMax CONSTANT)

public:
    bool getIsInitialized() const { return isInitialized; }
    int getMin() const { return min; }
    int getMax() const { return max; }

    bool isInitialized = false;
    int min = 0;
    int max = 0;
};

class JoystickDevice: public OsalDeviceListener
{
    Q_OBJECT

    friend class JoystickManager;

public:
    virtual ~JoystickDevice() override;

    Q_INVOKABLE QString id() const { return m_deviceInfo.id; }
    Q_INVOKABLE QString path() const { return m_deviceInfo.path; }
    Q_INVOKABLE QString modelName() const { return m_deviceInfo.modelName; }
    Q_INVOKABLE QString manufacturerName() const { return m_deviceInfo.manufacturerName; }

    Q_INVOKABLE int xAxisPosition() const { return m_xAxisPosition; }
    Q_INVOKABLE int yAxisPosition() const { return m_yAxisPosition; }
    Q_INVOKABLE int zAxisPosition() const { return m_zAxisPosition; }
    Q_INVOKABLE AxisDescrition xAxisDescription() const { return m_xAxisDescription; }
    Q_INVOKABLE AxisDescrition yAxisDescription() const { return m_yAxisDescription; }
    Q_INVOKABLE AxisDescrition zAxisDescription() const { return m_zAxisDescription; }

public:
    virtual void onStateChanged(const OsalDevice::State& newState) override;

signals:
    void stateChanged(const QList<bool>& state); //< This signal is for using in QML.

private:
    JoystickDevice(JoystickDeviceInfo deviceInfo, QObject* parent);

private:
    JoystickDeviceInfo m_deviceInfo;

    int m_xAxisPosition = 0;
    int m_yAxisPosition = 0;
    int m_zAxisPosition = 0;

    AxisDescrition m_xAxisDescription;
    AxisDescrition m_yAxisDescription;
    AxisDescrition m_zAxisDescription;
};

} // namespace nx::vms::client::desktop::joystick
