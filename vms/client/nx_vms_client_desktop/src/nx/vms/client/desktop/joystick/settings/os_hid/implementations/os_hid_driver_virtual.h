// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QBitArray>
#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

#include "../os_hid_driver.h"

namespace nx::vms::client::desktop::joystick {

struct JoystickDescriptor;

class OsHidDriverVirtual: public OsHidDriver
{
public:
    OsHidDriverVirtual();
    virtual ~OsHidDriverVirtual() override;

    QList<OsHidDeviceInfo> deviceList() override;

    virtual void setupDeviceSubscriber(const QString& path, QObject* subscriber) override;
    virtual void removeDeviceSubscriber(QObject* subscriber) override;

    static void attachVirtualJoystick();
    static void detachVirtualJoystick();

    static void setVirtualJoystickState(const QBitArray& newState);
    static JoystickDescriptor* virtualJoystickConfig();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::joystick
