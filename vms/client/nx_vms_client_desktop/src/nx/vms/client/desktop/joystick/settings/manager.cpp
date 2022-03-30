// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "manager.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <nx/reflect/json.h>
#include <nx/utils/log/log_main.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <ui/workbench/workbench_context_aware.h>

#include "action_factory.h"
#include "device.h"
#include "descriptors.h"

#if defined(Q_OS_LINUX)
    #include "manager_linux.h"
#elif defined(Q_OS_WIN)
    #include "manager_win.h"
#else
    #include "manager_hid.h"
#endif

using namespace std::chrono;

namespace {

// Other constants.
constexpr milliseconds kEnumerationInterval = 2500ms;
constexpr milliseconds kUsbPollInterval = 100ms;

const QString kSettingsDirName("/hid_configs/");

} // namespace

namespace nx::vms::client::desktop::joystick {

Manager* Manager::create(QObject* parent)
{
#if defined(Q_OS_LINUX)
    return new ManagerLinux(parent);
#elif defined(Q_OS_WIN)
    return new ManagerWindows(parent);
#else
    return new ManagerHid(parent);
#endif
}

Manager::Manager(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    loadConfig();

    connect(&m_enumerateTimer, &QTimer::timeout, this, &Manager::enumerateDevices);
    m_enumerateTimer.setInterval(kEnumerationInterval);
    m_enumerateTimer.start();

    m_pollTimer.setInterval(kUsbPollInterval);
    m_pollTimer.start();
}

Manager::~Manager()
{
}

QList<DevicePtr> Manager::devices() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_devices.values();
}

JoystickDescriptor Manager::getDefaultDeviceDescription(const QString& id) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_defaultDeviceConfigs[id];
}

JoystickDescriptor Manager::getDeviceDescription(const QString& id) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_deviceConfigs[id];
}

void Manager::updateDeviceDescription(const JoystickDescriptor& config)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_deviceConfigs[config.id] = config;
    for (auto& actionFactory: m_actionFactories)
    {
        if (actionFactory->id() == config.id)
            actionFactory->updateConfig(config);
    }

    const auto device = std::find_if(m_devices.begin(), m_devices.end(),
        [config] (const DevicePtr& device)
        {
            return device->id() == config.id;
        });

    if (device != m_devices.end())
        (*device)->updateStickAxisLimits(config);
}

void Manager::setDeviceActionsEnabled(bool enabled)
{
    m_deviceActionsEnabled = enabled;
}

void Manager::loadConfig()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    m_defaultDeviceConfigs.clear();
    m_deviceConfigRelativePath.clear();
    loadConfig(":" + kSettingsDirName,
        m_defaultDeviceConfigs,
        m_deviceConfigRelativePath);

    m_deviceConfigs = m_defaultDeviceConfigs;
    loadConfig(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + kSettingsDirName,
        m_deviceConfigs,
        m_deviceConfigRelativePath);
}

void Manager::saveConfig(const QString& id)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    const auto configPathIter = m_deviceConfigRelativePath.find(id);
    if (configPathIter == m_deviceConfigRelativePath.end())
    {
        NX_ASSERT(false, "Invalid joystick device id.");
        return;
    }

    const QString absoluteConfigPath =
        QStandardPaths::writableLocation(QStandardPaths::DataLocation) + *configPathIter;

    const QString dirPath = QFileInfo(absoluteConfigPath).dir().path();

    if (!QDir().mkpath(dirPath))
    {
        NX_WARNING(this, "Cannot create HID config directory: %1", dirPath);
        return;
    }

    QFile file(absoluteConfigPath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        NX_WARNING(this, "Cannot open HID config file: %1", absoluteConfigPath);
        return;
    }

    const auto configIter = m_deviceConfigs.find(id);
    if (configIter == m_deviceConfigs.end())
    {
        NX_ASSERT(false, "Invalid joystick device id.");
        return;
    }

    file.write(QByteArray::fromStdString(
        nx::reflect::json::serialize(*configIter)));
}

void Manager::loadConfig(
    const QString& searchDir,
    DeviceConfigs& destConfigs,
    QMap<QString, QString>& destIdToRelativePath) const
{
    for (const auto& fileInfo: QDir(searchDir).entryInfoList(QDir::Files))
    {
        QFile file(fileInfo.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        JoystickDescriptor config;
        if (!nx::reflect::json::deserialize<JoystickDescriptor>(
            file.readAll().toStdString(), &config))
        {
            continue;
        }

        destConfigs[config.id] = config;

        const QString tempAbsPath = QFileInfo(file).absoluteFilePath();
        const int startPos = tempAbsPath.indexOf(kSettingsDirName);
        destIdToRelativePath[config.id] = tempAbsPath.mid(startPos);
    }
}

void Manager::removeUnpluggedJoysticks(const QSet<QString>& foundDevicePaths)
{
    for (auto it = m_devices.begin(); it != m_devices.end(); )
    {
        const auto path = it.key();
        if (!foundDevicePaths.contains(path))
        {
            NX_VERBOSE(this, "Joystick has been removed: %1", path);

            // Shared pointer is used here, so the data wouldn't be invalidated.
            emit deviceDisconnected(it.value());

            it = m_devices.erase(it);
            m_actionFactories.remove(path);
        }
        else
        {
            ++it;
        }
    }
}

void Manager::initializeDevice(
    DevicePtr& device,
    const JoystickDescriptor& description,
    const QString& devicePath)
{
    auto factory = ActionFactoryPtr(new ActionFactory(description, this));

    connect(device.get(), &Device::stateChanged, this,
        [this, factory](const Device::StickPositions& stick, const Device::ButtonStates& buttons)
        {
            if (m_deviceActionsEnabled)
                factory->handleStateChanged(stick, buttons);
        });

    connect(factory.get(), &ActionFactory::actionReady, menu(),
        [this](ui::action::IDType id, const ui::action::Parameters& parameters)
        {
            menu()->triggerIfPossible(id, parameters);
        });

    m_actionFactories[devicePath] = factory;

    m_devices[devicePath] = device;

    emit deviceConnected(device);
}

} // namespace nx::vms::client::desktop::joystick
