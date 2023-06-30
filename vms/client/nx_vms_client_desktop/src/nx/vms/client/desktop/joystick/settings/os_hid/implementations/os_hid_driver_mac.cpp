// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "os_hid_driver_mac.h"

#include <QtCore/QMap>
#include <QtCore/QQueue>
#include <QtCore/QScopedPointer>
#include <QtCore/QSet>
#include <QtCore/QTimer>

#include <hidapi/hidapi.h>
#include <nx/utils/log/log.h>

#include "os_hid_device_mac.h"

using namespace std::chrono;

namespace {

// Supported Usages.
const QSet<uint16_t> kJoystickUsages = { 0x04, 0x05 };

// Supported HID Usage Pages.
constexpr uint16_t kGenericDesktopPage = 0x01;

constexpr milliseconds kEnumerationInterval = 2500ms;

} // namespace

namespace nx::vms::client::desktop::joystick {

static OsHidDeviceInfo loadDeviceInfo(hid_device_info* dev)
{
    return
        {
            .id = QString::number(dev->vendor_id, 16).rightJustified(4, '0') + "_"
                  + QString::number(dev->product_id, 16).rightJustified(4, '0'),
            .path = QString(dev->path),
            .modelName = QString::fromWCharArray(dev->product_string),
            .manufacturerName = QString::fromWCharArray(dev->manufacturer_string)
        };
}

struct OsHidDriverMac::Private
{
    QTimer* pollingTimer = nullptr;
    QMap<QString, OsHidDevice*> devices;
    QMap<QString, OsHidDeviceInfo> deviceInfos;
    QSet<QString> virtualDevices;
    QQueue<OsHidDevice*> attachingVirtualDevices;
    QQueue<QString> detachingVirtualDevicePaths;

    struct Subscription
    {
        const OsHidDeviceSubscriber* subscriber;
        QMetaObject::Connection connection;
    };

    QMap<QString, QList<Subscription>> deviceSubscribers;

    // TODO: Pack devices, deviceInfos and deviceSubscribers into single structure.

    QSet<QString> deviceKeys() const
    {
        auto keys = devices.keys();
        return QSet<QString>(keys.begin(), keys.end());
    }

    void processRealDeviceDetection(const OsHidDeviceInfo& deviceInfo, bool& devicesChanged)
    {
        OsHidDevice* hidDevice;
        auto hidDeviceIt = devices.find(deviceInfo.path);

        if (hidDeviceIt == devices.end())
        {
            NX_DEBUG(this, "New HID device detected: %1 %2 (%3)", deviceInfo.manufacturerName,
                deviceInfo.modelName, deviceInfo.path);

            hidDevice = new OsHidDeviceMac(deviceInfo);

            deviceInfos[deviceInfo.path] = deviceInfo;
            devices[deviceInfo.path] = hidDevice;

            devicesChanged = true;
        }
        else
        {
            hidDevice = *hidDeviceIt;
        }

        if (deviceSubscribers.contains(deviceInfo.path))
            updateDeviceSubscriptions(hidDevice);
    }

    void processVirtualDeviceDetection(const OsHidDeviceInfo& deviceInfo)
    {
        OsHidDevice* hidDevice;
        auto hidDeviceIt = devices.find(deviceInfo.path);

        if (hidDeviceIt == devices.end())
            return;

        hidDevice = *hidDeviceIt;

        if (deviceSubscribers.contains(deviceInfo.path))
            updateDeviceSubscriptions(hidDevice);
    }

    void addVirtualDevice(OsHidDevice* device)
    {
        const auto& deviceInfo = device->info();

        NX_DEBUG(this, "New virtual HID device detected: %1 %2 (%3)", deviceInfo.manufacturerName,
            deviceInfo.modelName, deviceInfo.path);

        deviceInfos[deviceInfo.path] = deviceInfo;
        devices[deviceInfo.path] = device;
        virtualDevices.insert(deviceInfo.path);

        if (deviceSubscribers.contains(deviceInfo.path))
            updateDeviceSubscriptions(device);
    }

    void updateDeviceSubscriptions(OsHidDevice* device)
    {
        const auto& deviceInfo = device->info();

        for (auto& subscription: deviceSubscribers[deviceInfo.path])
        {
            if (subscription.connection)
                continue;

            if (!device->isOpened())
            {
                if (!device->open())
                {
                    NX_DEBUG(this, "Failed to open HID device: %1 %2 (%3), ignoring.",
                        deviceInfo.manufacturerName,
                        deviceInfo.modelName,
                        deviceInfo.path);
                    continue;
                }
            }

            subscription.connection = connect(device, &OsHidDevice::stateChanged,
                subscription.subscriber, &OsHidDeviceSubscriber::onStateChanged);
        }
    }
};

OsHidDriverMac::OsHidDriverMac():
    d(new Private())
{
    hid_init();

    d->pollingTimer = new QTimer(this);
    d->pollingTimer->setInterval(kEnumerationInterval);
    connect(d->pollingTimer, &QTimer::timeout, this, &OsHidDriverMac::enumerateDevices);
    d->pollingTimer->start();
}

OsHidDriverMac::~OsHidDriverMac()
{
    delete d->pollingTimer;

    for (auto device: d->devices)
    {
        const auto& path = device->info().path;
        if (d->deviceSubscribers.contains(path))
        {
            for (const auto& subscription: d->deviceSubscribers[path])
            {
                if (subscription.connection)
                    disconnect(subscription.connection);
            }
        }

        device->stall();
        device->deleteLater();
    }

    d->deviceSubscribers.clear();

    hid_exit();
}

struct HidDeviceInfoScopedPointerCustomDeleter
{
    static inline void cleanup(hid_device_info* pointer)
    {
        hid_free_enumeration(pointer);
    }
};

using OsLevelDeviceInfoPtr =
    QScopedPointer<hid_device_info, HidDeviceInfoScopedPointerCustomDeleter>;

void OsHidDriverMac::enumerateDevices()
{
    bool devicesChanged = false;
    OsLevelDeviceInfoPtr osLevelDeviceInfos(hid_enumerate(/*vendorId*/ 0x0, /*productId*/ 0x0));

    QSet<QString> previousDevicePaths = d->deviceKeys();
    QSet<QString> actualDevicePaths;

    for (hid_device_info* osLevelDeviceInfo = osLevelDeviceInfos.get();
        osLevelDeviceInfo;
        osLevelDeviceInfo = osLevelDeviceInfo->next)
    {
        const OsHidDeviceInfo deviceInfo = loadDeviceInfo(osLevelDeviceInfo);

        if (osLevelDeviceInfo->usage_page != kGenericDesktopPage
            || kJoystickUsages.contains(osLevelDeviceInfo->usage))
        {
            continue;
        }

        actualDevicePaths.insert(deviceInfo.path);

        d->processRealDeviceDetection(deviceInfo, devicesChanged);
    }

    for (const auto& devicePath: d->virtualDevices)
    {
        const OsHidDeviceInfo deviceInfo = d->deviceInfos[devicePath];

        actualDevicePaths.insert(deviceInfo.path);

        d->processVirtualDeviceDetection(deviceInfo);
    }

    while (!d->attachingVirtualDevices.isEmpty())
    {
        const auto device = d->attachingVirtualDevices.dequeue();

        actualDevicePaths.insert(device->info().path);

        d->addVirtualDevice(device);

        devicesChanged = true;
    }

    auto disappearedDevicePaths = previousDevicePaths - actualDevicePaths;

    while (!d->detachingVirtualDevicePaths.isEmpty())
    {
        const auto devicePath = d->detachingVirtualDevicePaths.dequeue();
        disappearedDevicePaths.insert(devicePath);
    }

    for (const auto& devicePath: disappearedDevicePaths)
    {
        const bool isDeviceVirtual = d->virtualDevices.contains(devicePath);
        OsHidDevice* device = d->devices.take(devicePath);

        if (d->deviceSubscribers.contains(devicePath))
        {
            if (isDeviceVirtual)
            {
                for (auto& subscription: d->deviceSubscribers[devicePath])
                {
                    if (subscription.connection)
                        disconnect(subscription.connection);
                }
            }
            else
            {
                d->deviceSubscribers.remove(devicePath);
            }
        }

        d->deviceInfos.remove(devicePath);
        d->devices.remove(devicePath);

        if (isDeviceVirtual)
            d->virtualDevices.remove(devicePath);

        NX_DEBUG(this, "HID device disappeared: %1 %2 (%3)", device->info().manufacturerName,
            device->info().modelName, device->info().path);

        if (!isDeviceVirtual)
        {
            device->stall();
            device->deleteLater();
        }

        devicesChanged = true;
    }

    if (devicesChanged)
        emit deviceListChanged();
}

QList<OsHidDeviceInfo> OsHidDriverMac::deviceList()
{
    return d->deviceInfos.values();
}

void OsHidDriverMac::setupDeviceSubscriber(const QString& path,
    const OsHidDeviceSubscriber* subscriber)
{
    if (!d->deviceSubscribers.contains(path))
    {
        d->deviceSubscribers.insert(path, {{.subscriber = subscriber}});
    }
    else
    {
        auto it = std::find_if(
            d->deviceSubscribers[path].begin(),
            d->deviceSubscribers[path].end(),
            [subscriber](const Private::Subscription& subscription)
            {
                return subscription.subscriber == subscriber;
            });

        if (it == d->deviceSubscribers[path].end())
            d->deviceSubscribers[path].append({{.subscriber = subscriber}});
    }
}

void OsHidDriverMac::removeDeviceSubscriber(const OsHidDeviceSubscriber* subscriber)
{
    for (auto& subscribers: d->deviceSubscribers)
    {
        const auto& it = std::find_if(subscribers.begin(), subscribers.end(),
            [subscriber](const Private::Subscription& subscription)
            {
                return subscription.subscriber == subscriber;
            });

        if (it != subscribers.end())
            subscribers.erase(it);
    }
}

void OsHidDriverMac::attachVirtualDevice(OsHidDevice* device)
{
    const auto devicePath = device->info().path;

    NX_ASSERT(!d->attachingVirtualDevices.contains(device));
    d->attachingVirtualDevices.enqueue(device);
}

void OsHidDriverMac::detachVirtualDevice(OsHidDevice* device)
{
    const auto devicePath = device->info().path;

    NX_ASSERT(!d->detachingVirtualDevicePaths.contains(devicePath));
    d->detachingVirtualDevicePaths.enqueue(devicePath);
}

} // namespace nx::vms::client::desktop::joystick
