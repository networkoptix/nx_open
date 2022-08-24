// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "manager.h"

#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QMap>
#include <QtCore/QStandardPaths>
#include <QtGui/QGuiApplication>

#include <nx/reflect/json.h>
#include <nx/utils/log/log_main.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>

#include "action_factory.h"
#include "device.h"
#include "descriptors.h"

#if defined(Q_OS_LINUX)
    #include "manager_linux.h"
#elif defined(Q_OS_WINDOWS)
    #include "manager_win.h"
#else
    #include "manager_hid.h"
#endif

using namespace std::chrono;

namespace nx::vms::client::desktop::joystick {

namespace {

enum class SearchState
{
    /** Client just started, try to find a joystick quickly. */
    initial,

    /** No joysticks found, repeat search periodically. */
    periodic,

    /** Joystick is found, stop search. */
    idle,
};

// Other constants.
static const QMap<SearchState, milliseconds> kSearchIntervals{
    {SearchState::initial, 2500ms},
    {SearchState::periodic, 10s}
};

constexpr milliseconds kUsbPollInterval = 100ms;
constexpr milliseconds kInitialSearchPeriod = 20s;

const QString kSettingsDirName("/hid_configs/");

} // namespace

struct Manager::Private
{
    Manager* const q;

    // Configs for known device models.
    DeviceConfigs defaultDeviceConfigs;
    std::map<QString, nx::utils::ScopedConnections> deviceConnections;
    QMap<QString, QString> deviceConfigRelativePath;
    QMap<QString, ActionFactoryPtr> actionFactories;
    bool deviceActionsEnabled = true;

    SearchState searchState = SearchState::initial;
    QTimer* const enumerateTimer;
    QTimer* const pollTimer;
    QElapsedTimer initialSearchTimer;

    void updateSearchState()
    {
        SearchState targetState = searchState;
        const bool devicesFound = !q->devices().empty();

        switch (searchState)
        {
            case SearchState::initial:
            {
                if (devicesFound)
                    targetState = SearchState::idle;
                else if (!initialSearchTimer.isValid())
                    initialSearchTimer.start();
                else if (initialSearchTimer.hasExpired(kInitialSearchPeriod.count()))
                    targetState = SearchState::periodic;
                break;
            }
            case SearchState::periodic:
            {
                // If joystick was found, stop search.
                if (devicesFound)
                    targetState = SearchState::idle;
                break;
            }
            case SearchState::idle:
            {
                // If joystick was disconnected, switch to quick search.
                if (!devicesFound)
                    targetState = SearchState::initial;
                break;
            }
        }

        if (targetState != searchState)
        {
            searchState = targetState;
            initialSearchTimer.invalidate();
            enumerateTimer->stop();
            if (searchState != SearchState::idle)
            {
                enumerateTimer->setInterval(kSearchIntervals[searchState]);
                enumerateTimer->start();
            }
            NX_VERBOSE(this, "Switch to search state %1", (int)searchState);
        }
    }
};

Manager* Manager::create(QObject* parent)
{
#if defined(Q_OS_LINUX)
    return new ManagerLinux(parent);
#elif defined(Q_OS_WINDOWS)
    return new ManagerWindows(parent);
#else
    return new ManagerHid(parent);
#endif
}

Manager::Manager(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_mutex(nx::Mutex::Recursive),
    d(new Private{
        .q = this,
        .enumerateTimer = new QTimer(this),
        .pollTimer = new QTimer(this)
    })
{
    loadConfig();

    connect(d->enumerateTimer, &QTimer::timeout, this,
        [this]()
        {
            enumerateDevices();
            d->updateSearchState();
        });
    d->enumerateTimer->setInterval(kSearchIntervals[d->searchState]);
    d->enumerateTimer->start();
    d->initialSearchTimer.start();

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

QList<DevicePtr> Manager::devices() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_devices.values();
}

JoystickDescriptor Manager::getDefaultDeviceDescription(const QString& modelName) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return d->defaultDeviceConfigs[modelName];
}

JoystickDescriptor Manager::getDeviceDescription(const QString& modelName) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_deviceConfigs[modelName];
}

void Manager::updateDeviceDescription(const JoystickDescriptor& config)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_deviceConfigs[config.model] = config;
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

void Manager::loadConfig()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    d->defaultDeviceConfigs.clear();
    d->deviceConfigRelativePath.clear();
    loadConfig(":" + kSettingsDirName,
        d->defaultDeviceConfigs,
        d->deviceConfigRelativePath);

    m_deviceConfigs = d->defaultDeviceConfigs;
    loadConfig(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + kSettingsDirName,
        m_deviceConfigs,
        d->deviceConfigRelativePath);
}

void Manager::saveConfig(const QString& model)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    const auto configPathIter = d->deviceConfigRelativePath.find(model);
    if (configPathIter == d->deviceConfigRelativePath.end())
    {
        NX_ASSERT(false, "Invalid joystick device model.");
        return;
    }

    const QString absoluteConfigPath =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + *configPathIter;

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

    const auto configIter = m_deviceConfigs.find(model);
    if (configIter == m_deviceConfigs.end())
    {
        NX_ASSERT(false, "Invalid joystick device model.");
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

        destConfigs[config.model] = config;

        const QString tempAbsPath = QFileInfo(file).absoluteFilePath();
        const int startPos = tempAbsPath.indexOf(kSettingsDirName);
        destIdToRelativePath[config.model] = tempAbsPath.mid(startPos);
    }
}

void Manager::removeUnpluggedJoysticks(const QSet<QString>& foundDevicePaths)
{
    bool deviceWasRemoved = false;

    for (auto it = m_devices.begin(); it != m_devices.end(); )
    {
        const auto path = it.key();
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
        d->updateSearchState();
}

void Manager::initializeDevice(
    DevicePtr& device,
    const JoystickDescriptor& description,
    const QString& devicePath)
{
    auto factory = ActionFactoryPtr(new ActionFactory(description, this));

    d->deviceConnections[devicePath] <<
        connect(device.get(), &Device::stateChanged, this,
            [this, factory](const Device::StickPosition& stick, const Device::ButtonStates& buttons)
            {
                if (d->deviceActionsEnabled)
                    factory->handleStateChanged(stick, buttons);
            });

    d->deviceConnections[devicePath] <<
        connect(factory.get(), &ActionFactory::actionReady, menu(),
            [this](ui::action::IDType id, const ui::action::Parameters& parameters)
            {
                menu()->triggerIfPossible(id, parameters);
            });

    d->actionFactories[devicePath] = factory;

    m_devices[devicePath] = device;
}

QTimer* Manager::pollTimer() const
{
    return d->pollTimer;
}

} // namespace nx::vms::client::desktop::joystick
