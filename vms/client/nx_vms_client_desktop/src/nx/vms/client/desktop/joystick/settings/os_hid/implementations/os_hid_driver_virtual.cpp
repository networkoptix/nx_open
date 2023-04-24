// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "os_hid_driver_virtual.h"

#include <algorithm>

#include <nx/reflect/json.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/joystick/settings/descriptors.h>

#include "os_hid_device_virtual.h"

namespace {

const auto virtualJoystickConfigPath = ":/hid_configs/virtual.json";

} // namespace

namespace nx::vms::client::desktop::joystick {

struct OsHidDriverVirtual::Private
{
    nx::Mutex m_mutex;
    OsHidDeviceVirtual* device = nullptr;
    struct Subscription
    {
        QObject* subscriber;
        bool isConnected = false;
    };

    QList<Subscription> deviceSubscribers;

    void updateSubscriptions();
};

void OsHidDriverVirtual::Private::updateSubscriptions()
{
    if (!device)
        return;

    for (auto& subscription: deviceSubscribers)
    {
        if (!subscription.isConnected)
        {
            connect(device, SIGNAL(stateChanged(const QBitArray&)),
                subscription.subscriber, SLOT(onStateChanged(const QBitArray&)));
            subscription.isConnected = true;
        }
    }
}

OsHidDriverVirtual::OsHidDriverVirtual():
    d(new Private())
{
}

OsHidDriverVirtual::~OsHidDriverVirtual()
{
}

QList<OsHidDeviceInfo> OsHidDriverVirtual::deviceList()
{
    if (d->device)
        return {OsHidDeviceVirtual::deviceInfo()};
    else
        return {};
}

void OsHidDriverVirtual::setupDeviceSubscriber(const QString& path, QObject* subscriber)
{
    if (path != OsHidDeviceVirtual::deviceInfo().path)
        return;

    const auto& it = std::find_if(d->deviceSubscribers.begin(), d->deviceSubscribers.end(),
        [subscriber](const Private::Subscription& subscription)
        {
            return subscription.subscriber == subscriber;
        });

    if (it == d->deviceSubscribers.end())
        d->deviceSubscribers.append({.subscriber = subscriber});

    d->updateSubscriptions();
}

void OsHidDriverVirtual::removeDeviceSubscriber(QObject* subscriber)
{
    const auto& it = std::find_if(d->deviceSubscribers.begin(), d->deviceSubscribers.end(),
        [subscriber](const Private::Subscription& subscription)
        {
            return subscription.subscriber == subscriber;
        });

    if (it != d->deviceSubscribers.end())
        d->deviceSubscribers.erase(it);
}

void OsHidDriverVirtual::attachVirtualJoystick()
{
    auto driverOfVirtualDevices = dynamic_cast<OsHidDriverVirtual*>(OsHidDriver::getDriver());

    if (!driverOfVirtualDevices)
    {
        NX_WARNING(NX_SCOPE_TAG, "There is no virtual joystick driver.");
        return;
    }

    NX_MUTEX_LOCKER lock(&driverOfVirtualDevices->d->m_mutex);

    if (driverOfVirtualDevices->d->device)
    {
        NX_WARNING(NX_SCOPE_TAG, "Virtual joystick is already attached.");
        return;
    }

    driverOfVirtualDevices->d->device = new OsHidDeviceVirtual();

    emit driverOfVirtualDevices->deviceListChanged();

    NX_DEBUG(NX_SCOPE_TAG, "Virtual joystick attached.");
}

void OsHidDriverVirtual::detachVirtualJoystick()
{
    auto driverOfVirtualDevices = dynamic_cast<OsHidDriverVirtual*>(OsHidDriver::getDriver());

    if (!driverOfVirtualDevices)
    {
        NX_WARNING(NX_SCOPE_TAG, "OsHidDriver is not OsHidDriverVirtual.");
        return;
    }

    NX_MUTEX_LOCKER lock(&driverOfVirtualDevices->d->m_mutex);

    if (!driverOfVirtualDevices->d->device)
    {
        NX_WARNING(NX_SCOPE_TAG, "Virtual joystick is already detached.");
        return;
    }

    driverOfVirtualDevices->d->deviceSubscribers.clear();
    driverOfVirtualDevices->d->device->deleteLater();
    driverOfVirtualDevices->d->device = nullptr;

    emit driverOfVirtualDevices->deviceListChanged();

    NX_DEBUG(NX_SCOPE_TAG, "Virtual joystick detached.");
}

void OsHidDriverVirtual::setVirtualJoystickState(const QBitArray& newState)
{
    auto driverOfVirtualDevices = dynamic_cast<OsHidDriverVirtual*>(OsHidDriver::getDriver());

    if (!driverOfVirtualDevices)
    {
        NX_WARNING(driverOfVirtualDevices, "OsHidDriver is not OsHidDriverVirtual.");
        return;
    }

    NX_MUTEX_LOCKER lock(&driverOfVirtualDevices->d->m_mutex);

    auto device = dynamic_cast<OsHidDeviceVirtual*>(driverOfVirtualDevices->d->device);

    NX_ASSERT(device);

    if (device->state() != newState)
        device->setState(newState);
}

JoystickDescriptor* OsHidDriverVirtual::virtualJoystickConfig()
{
    return OsHidDeviceVirtual::joystickConfig();
}

} // namespace nx::vms::client::desktop::joystick
