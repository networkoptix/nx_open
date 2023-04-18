// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "joystick_manager.h"

#include <QtQml/QtQml>

#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/joystick/settings/os_hid/os_hid_driver.h>
#include <ui/workbench/workbench_context.h>

#include "joystick_device.h"

namespace nx::vms::client::desktop::joystick {

struct JoystickManager::Private
{
    struct JoystickData
    {
        JoystickDevice* device = nullptr;
        OsHidDeviceInfo info;

        JoystickDevice* createDevice(QObject* parent)
        {
            if (device)
                return device;

            device = new JoystickDevice(info, parent);

            return device;
        }
    };

    QMap<QString, JoystickData> joysticks;

    QSet<QString> devicePaths() const
    {
        QSet<QString> result;
        for (auto it = joysticks.begin(); it != joysticks.end(); ++it)
            result.insert(it.key());
        return result;
    }
};

JoystickManager::JoystickManager():
    d(new Private())
{
    connect(OsHidDriver::getDriver(), &OsHidDriver::deviceListChanged,
        this, &JoystickManager::onHidDeviceListChanged);
}

JoystickDevice* JoystickManager::createDevice(const QString& path)
{
    if (!d->joysticks.contains(path))
        return nullptr;

    return d->joysticks[path].createDevice(this);
}

JoystickManager::~JoystickManager()
{
}

void JoystickManager::onHidDeviceListChanged()
{
    const QSet<QString> previousDeviceKeys = d->devicePaths();
    QSet<QString> detectedDeviceKeys;

    for (const auto& deviceInfo: OsHidDriver::getDriver()->deviceList())
    {
        detectedDeviceKeys.insert(deviceInfo.path);

        if (d->joysticks.contains(deviceInfo.path))
            return;

        NX_DEBUG(this, "Detected device: %1 %2 (%3)", deviceInfo.manufacturerName,
            deviceInfo.modelName, deviceInfo.id);

        d->joysticks[deviceInfo.path] = {
            .info = deviceInfo,
        };
    }

    for (const auto& key: previousDeviceKeys - detectedDeviceKeys)
    {
        NX_DEBUG(this, "Device removed: %1 %2 (%3)",
            d->joysticks[key].info.manufacturerName,
            d->joysticks[key].info.modelName,
            d->joysticks[key].info.id);

        d->joysticks.remove(key);
    }

    emit deviceListChanged();
}

QVariantMap JoystickManager::devices() const
{
    QVariantMap result;

    for (auto it = d->joysticks.begin(); it != d->joysticks.end(); ++it)
    {
        result[it.key()] = QString("%1 - %2 (%3)")
            .arg(it.value().info.manufacturerName, it.value().info.modelName, it.value().info.id);
    }

    return result;
}

void JoystickManager::registerQmlType()
{
    qmlRegisterUncreatableType<JoystickManager>("nx.vms.client.desktop", 1, 0,
        "JoystickManager",
        "JoystickManager can be created from C++ code only.");

    qmlRegisterUncreatableType<JoystickDevice>("nx.vms.client.desktop", 1, 0,
        "JoystickDevice",
        "JoystickDevice can be created from C++ code only.");
}

} // namespace nx::vms::client::desktop::joystick
