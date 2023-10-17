// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "manager.h"

#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QMap>
#include <QtCore/QStandardPaths>
#include <QtGui/QGuiApplication>

#include <nx/reflect/json.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/string.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/window_context.h>

#include "action_factory.h"
#include "descriptors.h"

#if defined(Q_OS_LINUX)
    #include "manager_linux.h"
#elif defined(Q_OS_WINDOWS)
    #include "manager_win.h"
    #include "non_blocking_manager_win.h"
#else
    #include "manager_hid.h"
#endif

using namespace std::chrono;

namespace nx::vms::client::desktop::joystick {

namespace {

constexpr milliseconds kUsbPollInterval = 100ms;

const QString kSettingsDirName("/hid_configs/");
const QString kGeneralConfigFileName("general_device.json");
const QString kGeneralConfigFileNameTemplate("general_device_%1.json");

} // namespace

struct Manager::Private
{
    Manager* const q;

    // Config for unknown device models.
    JoystickDescriptor defaultGeneralDeviceConfig;

    // Configs for known device models.
    DeviceConfigs defaultDeviceConfigs;
    DeviceConfigs deviceConfigs;

    std::map<QString, nx::utils::ScopedConnections> deviceConnections;
    QMap<QString, QString> deviceConfigFileNames;
    QMap<QString, ActionFactoryPtr> actionFactories;
    bool deviceActionsEnabled = true;

    QTimer* const pollTimer;
};

Manager* Manager::create(QObject* parent)
{
#if defined(Q_OS_LINUX)
    return new ManagerLinux(parent);
#elif defined(Q_OS_WINDOWS)
    if (ini().joystickPollingInSeparateThread)
        return new NonBlockingManagerWin(parent);
    else
        return new ManagerWindows(parent);
#else
    return new ManagerHid(parent);
#endif
}

Manager::Manager(QObject* parent):
    base_type(parent),
    m_mutex(nx::Mutex::Recursive),
    d(new Private{
        .q = this,
        .pollTimer = new QTimer(this)
    })
{
    loadConfigs();

    d->pollTimer->setInterval(kUsbPollInterval);
    d->pollTimer->start();

    connect(qApp, &QGuiApplication::applicationStateChanged,
        [this](Qt::ApplicationState state)
        {
            if (state == Qt::ApplicationActive)
            {
                d->pollTimer->start();
            }
            else
            {
                d->pollTimer->stop();

                for (auto& device: m_devices)
                    device->resetState();
            }
        });
}

Manager::~Manager()
{
}

QList<const Device*> Manager::devices() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    QList<const Device*> result;
    for (const auto& device: m_devices)
        result.push_back(device.get());

    return result;
}

JoystickDescriptor Manager::createDeviceDescription(const QString& modelName)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    const auto configIter = d->deviceConfigs.find(modelName);
    if (d->deviceConfigs.end() != configIter)
        return *configIter;

    JoystickDescriptor result = d->defaultGeneralDeviceConfig;
    result.model = modelName;

    d->deviceConfigs[modelName] = result;
    d->deviceConfigFileNames[modelName] = kGeneralConfigFileNameTemplate.arg(
        nx::utils::replaceNonFileNameCharacters(modelName, ' '));

    return result;
}

JoystickDescriptor Manager::getDefaultDeviceDescription(const QString& modelName) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    const auto configIter = d->defaultDeviceConfigs.find(modelName);
    if (d->defaultDeviceConfigs.end() != configIter)
        return *configIter;

    JoystickDescriptor result = d->defaultGeneralDeviceConfig;
    result.model = modelName;

    return result;
}

const JoystickDescriptor& Manager::getDeviceDescription(const QString& modelName) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    const auto configIter = d->deviceConfigs.find(modelName);
    if (d->deviceConfigs.end() != configIter)
        return *configIter;

    return d->defaultGeneralDeviceConfig;
}

void Manager::updateDeviceDescription(const JoystickDescriptor& config)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    d->deviceConfigs[config.model] = config;

    for (auto& actionFactory: d->actionFactories)
    {
        if (actionFactory->modelName() == config.model)
            actionFactory->updateConfig(config);
    }

    const auto device = std::find_if(m_devices.begin(), m_devices.end(),
        [config] (const DevicePtr& device)
        {
            return device->modelName() == config.model;
        });

    if (device != m_devices.end())
        (*device)->updateStickAxisLimits(config);
}

void Manager::setDeviceActionsEnabled(bool enabled)
{
    d->deviceActionsEnabled = enabled;
}

void Manager::saveConfig(const QString& model)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    const auto configFileNameIter = d->deviceConfigFileNames.find(model);
    if (configFileNameIter == d->deviceConfigFileNames.end())
    {
        NX_ASSERT(false, "Invalid joystick device model.");
        return;
    }

    const QString absoluteConfigPath = getLocalConfigDirPath().absoluteFilePath(*configFileNameIter);

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

    const JoystickDescriptor description = getDeviceDescription(model);

    if (isGeneralJoystickConfig(description))
    {
        NX_ASSERT(false, "General config hasn't been initialized for the device %1",
            description.model);
    }

    file.write(QByteArray::fromStdString(nx::reflect::json::serialize(description)));
}

void Manager::updateSearchState()
{
}

void Manager::loadConfigs()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    d->defaultGeneralDeviceConfig = {};
    d->defaultDeviceConfigs.clear();
    d->deviceConfigFileNames.clear();

    loadGeneralConfig(":" + kSettingsDirName, d->defaultGeneralDeviceConfig);

    loadConfig({":" + kSettingsDirName},
        d->defaultDeviceConfigs,
        d->deviceConfigFileNames);

    d->deviceConfigs = d->defaultDeviceConfigs;

    loadConfig(getLocalConfigDirPath(),
        d->deviceConfigs,
        d->deviceConfigFileNames);
}

void Manager::loadConfig(
    const QDir& searchDir,
    DeviceConfigs& destConfigs,
    QMap<QString, QString>& destIdToFileName) const
{
    for (const auto& fileInfo: searchDir.entryInfoList(QDir::Files))
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

        if (fileInfo.fileName() == kGeneralConfigFileName)
            continue;

        destConfigs[config.model] = config;
        destIdToFileName[config.model] = QFileInfo(file).fileName();
    }
}

void Manager::loadGeneralConfig(const QDir& searchDir, JoystickDescriptor& destConfig) const
{
    QFile file(searchDir.filePath(kGeneralConfigFileName));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    JoystickDescriptor config;
    if (!nx::reflect::json::deserialize<JoystickDescriptor>(
        file.readAll().toStdString(), &config))
    {
        return;
    }

    destConfig = config;
}

void Manager::removeUnpluggedJoysticks(const QSet<QString>& foundDevicePaths)
{
    bool deviceWasRemoved = false;

    for (auto it = m_devices.begin(); it != m_devices.end(); )
    {
        const QString path = it.key();
        if (!foundDevicePaths.contains(path))
        {
            NX_VERBOSE(this, "Joystick has been removed: %1", path);

            it = m_devices.erase(it);
            d->deviceConnections.erase(path);
            d->actionFactories.remove(path);

            deviceWasRemoved = true;
        }
        else
        {
            ++it;
        }
    }

    // Restart search if needed.
    if (deviceWasRemoved)
        updateSearchState();
}

void Manager::initializeDevice(const DevicePtr& device, const JoystickDescriptor& description)
{
    auto factory = ActionFactoryPtr(new ActionFactory(description, this));

    d->deviceConnections[device->path()] <<
        connect(device.get(), &Device::stateChanged, this,
            [this, factory](const Device::StickPosition& stick, const Device::ButtonStates& buttons)
            {
                if (d->deviceActionsEnabled)
                    factory->handleStateChanged(stick, buttons);
            });

    d->deviceConnections[device->path()] <<
        connect(factory.get(), &ActionFactory::actionReady, this,
            [this](menu::IDType id, const menu::Parameters& parameters)
            {
                if (auto context = appContext()->mainWindowContext())
                    context->menu()->triggerIfPossible(id, parameters);
            });

    d->actionFactories[device->path()] = factory;

    m_devices[device->path()] = device;
}

bool Manager::isGeneralJoystickConfig(const JoystickDescriptor& config)
{
    return d->defaultGeneralDeviceConfig.model == config.model;
}

QTimer* Manager::pollTimer() const
{
    return d->pollTimer;
}

const DeviceConfigs& Manager::getKnownJoystickConfigs() const
{
    return d->deviceConfigs;
}

QDir Manager::getLocalConfigDirPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + kSettingsDirName;
}

} // namespace nx::vms::client::desktop::joystick
