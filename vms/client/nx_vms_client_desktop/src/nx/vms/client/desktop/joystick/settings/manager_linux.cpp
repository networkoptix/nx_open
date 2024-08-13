// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "manager_linux.h"

#include <fcntl.h>

#include <QtCore/QDir>
#include <QtGui/QGuiApplication>

#include <linux/joystick.h>
#include <nx/utils/log/log_main.h>

#include "descriptors.h"
#include "device_linux.h"

using namespace std::chrono;

namespace nx::vms::client::desktop::joystick {

namespace {

constexpr milliseconds kEnumerationInterval = 2500ms;

QString getConfigsDebugInfo(const DeviceConfigs& configs)
{
    QStringList results;

    int i = 0;
    for (const JoystickDescriptor& config: configs)
    {
        results.append(nx::format("%1) Manufacturer: %2, model: %3")
            .args(++i, config.manufacturer, config.model));
    }

    return results.join('\n');
}

} // namespace

ManagerLinux::ManagerLinux(QObject* parent):
    base_type(parent)
{
    connect(&m_enumerateTimer, &QTimer::timeout, this, &ManagerLinux::enumerateDevices);
    m_enumerateTimer.setInterval(kEnumerationInterval);
    m_enumerateTimer.start();

    connect(qApp, &QGuiApplication::applicationStateChanged,
        [this](Qt::ApplicationState state)
        {
            if (state == Qt::ApplicationActive)
            {
                NX_DEBUG(this, "Application became active. Resuming the joystick driver.");

                m_enumerateTimer.start();
            }
            else
            {
                NX_DEBUG(this, "Application became inactive. Halting the joystick driver.");

                m_enumerateTimer.stop();
            }
        });

    NX_VERBOSE(this, "Known joystick configs:\n%1", getConfigsDebugInfo(getKnownJoystickConfigs()));
}

void ManagerLinux::enumerateDevices()
{
    NX_TRACE(this, "Joystick enumeration started.");

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
        __u8 axesNumber;
        __u8 buttonsNumber;

        ioctl(fd, JSIOCGNAME(kNameMaxSize), name);
        ioctl(fd, JSIOCGVERSION, &version);
        ioctl(fd, JSIOCGAXES, &axesNumber);
        ioctl(fd, JSIOCGBUTTONS, &buttonsNumber);

        close(fd);

        const QString modelAndManufacturer(name);
        NX_VERBOSE(this,
            "A new Joystick has been found. "
            "Model: %1, path: %2",
            modelAndManufacturer, path);

        const auto model = findDeviceModel(modelAndManufacturer);
        if (model.isEmpty())
        {
            NX_VERBOSE(this, "Model is not found.");
            continue;
        }

        const auto config = createDeviceDescription(findDeviceModel(modelAndManufacturer));
        DeviceLinux* deviceLinux = new DeviceLinux(config, path, pollTimer(), this);
        DevicePtr device(deviceLinux);
        deviceLinux->setFoundControlsNumber(axesNumber, buttonsNumber);

        if (device->isValid())
            initializeDevice(device, config);
        else
            NX_VERBOSE(this, "Device is invalid. Model: %1, path: %2", modelAndManufacturer, path);
    }

    removeUnpluggedJoysticks(foundDevices);

    NX_TRACE(this, "Joystick enumeration finished.");
}

QString ManagerLinux::findDeviceModel(const QString& modelAndManufacturer) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    const auto knownConfigsList = getKnownJoystickConfigs();

    const auto iter = std::find_if(knownConfigsList.begin(),
        knownConfigsList.end(),
        [modelAndManufacturer](const JoystickDescriptor& config)
        {
            QString model = modelAndManufacturer;

            int manufacturerStart = model.indexOf(config.manufacturer);
            if (manufacturerStart != -1)
            {
                model.remove(manufacturerStart, config.manufacturer.size());
                model = model.trimmed();
            }

            return model == config.model;
        });

    return iter != knownConfigsList.end() ? iter->model : "";
}

} // namespace nx::vms::client::desktop::joystick
