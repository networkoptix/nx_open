// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "os_hid_driver_mac.h"

#include <QtCore/QMap>
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

constexpr int kMaxHidReportSize = 1024;

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
    QMap<QString, OsHidDeviceMac*> devices;
    QMap<QString, OsHidDeviceInfo> deviceInfos;

    struct Subscription
    {
        QObject* subscriber;
        bool isConnected = false;
    };

    QMap<QString, QList<Subscription>> deviceSubscribers;

    // TODO: Pack devices, deviceInfos and deviceSubscribers into single structure.

    QSet<QString> deviceKeys() const
    {
        auto keys = devices.keys();
        return QSet<QString>(keys.begin(), keys.end());
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
                disconnect(device, SIGNAL(stateChanged(QBitArray)), subscription.subscriber,
                    SLOT(onStateChanged(QBitArray)));
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
        OsHidDeviceInfo deviceInfo = loadDeviceInfo(osLevelDeviceInfo);

        if (osLevelDeviceInfo->usage_page != kGenericDesktopPage
            || kJoystickUsages.contains(osLevelDeviceInfo->usage))
        {
            continue;
        }

        actualDevicePaths.insert(deviceInfo.path);

        OsHidDeviceMac* hidDevice;
        auto hidDeviceIt = d->devices.find(deviceInfo.path);

        if (hidDeviceIt == d->devices.end())
        {
            NX_DEBUG(this, "New HID device detected: %1 %2 (%3)", deviceInfo.manufacturerName,
                deviceInfo.modelName, deviceInfo.path);

            hidDevice = new OsHidDeviceMac(deviceInfo);

            d->deviceInfos[deviceInfo.path] = OsHidDeviceInfo{deviceInfo.id, deviceInfo.path,
                deviceInfo.modelName, deviceInfo.manufacturerName};
            d->deviceInfos[deviceInfo.path] = deviceInfo;

            d->devices[deviceInfo.path] = hidDevice;

            devicesChanged = true;
        }
        else
        {
            hidDevice = *hidDeviceIt;
        }

        if (d->deviceSubscribers.contains(deviceInfo.path))
        {
            for (auto& subscription: d->deviceSubscribers[deviceInfo.path])
            {
                if (subscription.isConnected)
                    continue;

                if (!hidDevice->isOpened())
                {
                    if (!hidDevice->open())
                    {
                        NX_DEBUG(this, "Failed to open HID device: %1 %2 (%3), ignoring.",
                            deviceInfo.manufacturerName,
                            deviceInfo.modelName,
                            deviceInfo.path);
                        continue;
                    }
                }

                connect(hidDevice, SIGNAL(stateChanged(QBitArray)),
                    subscription.subscriber, SLOT(onStateChanged(QBitArray)));

                subscription.isConnected = true;
            }
        }
    }

    for (const auto& devicePath: previousDevicePaths - actualDevicePaths)
    {
        OsHidDeviceMac* device = d->devices.take(devicePath);

        if (d->deviceSubscribers.contains(devicePath))
            d->deviceSubscribers.remove(devicePath);

        d->deviceInfos.remove(devicePath);
        d->devices.remove(devicePath);

        NX_DEBUG(this, "HID device disappeared: %1 %2 (%3)", device->info().manufacturerName,
            device->info().modelName, device->info().path);

        device->stall();
        device->deleteLater();

        devicesChanged = true;
    }

    if (devicesChanged)
        emit deviceListChanged();
}

QList<OsHidDeviceInfo> OsHidDriverMac::deviceList()
{
    return d->deviceInfos.values();
}

void OsHidDriverMac::setupDeviceSubscriber(const QString& path, QObject* subscriber)
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

void OsHidDriverMac::removeDeviceSubscriber(QObject* subscriber)
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

} // namespace nx::vms::client::desktop::joystick
