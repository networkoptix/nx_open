// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "manager_hid.h"

#include <hidapi/hidapi.h>

#include <QtCore/QDir>

#include <nx/utils/log/log_main.h>

#include "device_hid.h"
#include "descriptors.h"

using namespace std::chrono;

namespace {

// Supported Usages.
constexpr uint16_t kJoystick = 0x04;

// Supported HID Usage Pages.
constexpr uint16_t kGenericDesktopPage = 0x01;

constexpr milliseconds kEnumerationInterval = 2500ms;

} // namespace

namespace nx::vms::client::desktop::joystick {

ManagerHid::ManagerHid(QObject* parent):
    base_type(parent)
{
    connect(&m_enumerateTimer, &QTimer::timeout, this, &ManagerHid::enumerateDevices);
    m_enumerateTimer.setInterval(kEnumerationInterval);
    m_enumerateTimer.start();

    hid_init();
}

ManagerHid::~ManagerHid()
{
    hid_exit();
}

void ManagerHid::enumerateDevices()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    QSet<QString> foundDevices;

    struct hid_device_info* devs = hid_enumerate(/*vendorId*/ 0x0, /*productId*/ 0x0);
    struct hid_device_info* currentDevice = devs;
    while (currentDevice)
    {
        // At the moment we support only Joystick devices, so the filtering is very simple.
        if (currentDevice->usage_page == kGenericDesktopPage && currentDevice->usage == kJoystick)
        {
            const QString path(currentDevice->path);
            foundDevices << path;

            if (!m_devices.contains(path))
            {
                const QString modelName =
                    QString::fromWCharArray(currentDevice->product_string);

                const QString manufacturerName =
                    QString::fromWCharArray(currentDevice->manufacturer_string);

                const auto id =
                    QString::number(currentDevice->vendor_id, 16).rightJustified(4, '0') + "_"
                    + QString::number(currentDevice->product_id, 16).rightJustified(4, '0');

                NX_VERBOSE(this,
                    "A new Joystick has been found. "
                    "Manufacturer: %1, model: %2, id: %3, path: %4",
                    manufacturerName, modelName, id, path);

                const auto config = getDeviceDescription(modelName);
                if (isGeneralJoystickConfig(config))
                {
                    NX_VERBOSE(this, "Unsupported device. Model: %1, path: %2", modelName, path);
                    return;
                }

                DevicePtr device(new DeviceHid(config, path, pollTimer()));
                if (device->isValid())
                    initializeDevice(device, config, path);
                else
                    NX_VERBOSE(this, "Device is invalid. Model: %1, path: %2", modelName, path);
            }
        }
        currentDevice = currentDevice->next;
    }

    removeUnpluggedJoysticks(foundDevices);

    hid_free_enumeration(devs);
}

} // namespace nx::vms::client::desktop::joystick
