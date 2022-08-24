// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "manager_linux.h"

#include <fcntl.h>
#include <linux/joystick.h>

#include <QtCore/QDir>

#include <nx/utils/log/log_main.h>

#include "device_linux.h"
#include "descriptors.h"

namespace nx::vms::client::desktop::joystick {

ManagerLinux::ManagerLinux(QObject* parent):
    base_type(parent)
{
}

void ManagerLinux::enumerateDevices()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    QSet<QString> foundDevices;

    QDir inputsDir("/dev/input/");
    inputsDir.setFilter(QDir::AllEntries | QDir::System);
    QFileInfoList joystickInputFiles = inputsDir.entryInfoList({"js*"});
    for (const auto& joystickInput: joystickInputFiles)
    {
        const QString path = joystickInput.absoluteFilePath();
        foundDevices << path;
        if (m_devices.contains(path))
            continue;

        int fd = open(joystickInput.absoluteFilePath().toLatin1().data(), O_RDONLY | O_NONBLOCK);
        if (fd == -1)
            continue;

        static constexpr int kNameMaxSize = 256;
        char name[kNameMaxSize];
        __u32 version;
        __u8 axes;
        __u8 buttons;

        ioctl(fd, JSIOCGNAME(kNameMaxSize), name);
        ioctl(fd, JSIOCGVERSION, &version);
        ioctl(fd, JSIOCGAXES, &axes);
        ioctl(fd, JSIOCGBUTTONS, &buttons);

        close(fd);

        const QString modelName(name);
        NX_VERBOSE(this,
            "A new Joystick has been found. "
            "Model: %1, path: %2",
            modelName, path);

        const auto iter = std::find_if(m_deviceConfigs.begin(), m_deviceConfigs.end(),
            [modelName](const JoystickDescriptor& description)
            {
                return modelName.contains(description.model);
            });

        if (iter != m_deviceConfigs.end())
        {
            const auto config = *iter;

            DevicePtr device(new DeviceLinux(config, path, pollTimer()));
            if (device->isValid())
                initializeDevice(device, config, path);
        }
        else
        {
            NX_VERBOSE(this,
                "An unsupported Joystick has been found. "
                "Model: %1, path: %2",
                modelName, path);
        }
    }

    removeUnpluggedJoysticks(foundDevices);
}

} // namespace nx::vms::client::desktop::joystick
