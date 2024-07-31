// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "manager_win.h"

#include <libloaderapi.h>

#include <QtCore/QElapsedTimer>

#include <nx/utils/log/log_main.h>
#include <nx/utils/qt_helpers.h>

#include "descriptors.h"
#include "device_win.h"

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

constexpr milliseconds kInitialSearchPeriod = 20s;

QString errorCodeToString(HRESULT code)
{
    LPVOID text = nullptr;

    ::FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
	    nullptr,
        code,
        MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
        (LPWSTR)&text,
        0,
        nullptr);

    const auto result = QString::fromWCharArray((LPCWSTR)text);

    LocalFree(text);

    return result;
}

} // namespace

struct ManagerWindows::Private
{
    ManagerWindows* const q;

    LPDIRECTINPUT8 directInput = nullptr; //< Actually, it must be single per app.

    QVector<FoundDeviceInfo> foundDevices;

    QMap<QString, DeviceWindowsPtr> intitializingDevices;

    SearchState searchState = SearchState::initial;
    QTimer* const enumerateTimer;
    QElapsedTimer initialSearchTimer;
};

ManagerWindows::ManagerWindows(QObject* parent):
    base_type(parent),
    d(new Private{
        .q = this,
        .enumerateTimer = new QTimer(this)
    })
{
    HRESULT status = DirectInput8Create(
        GetModuleHandle(nullptr),
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        reinterpret_cast<LPVOID*>(&d->directInput),
        nullptr);

    if (status != DI_OK)
        NX_WARNING(this, "DirectInput8Create failed: %1", errorCodeToString(status));

    connect(d->enumerateTimer, &QTimer::timeout, this,
        [this]()
        {
            enumerateDevices();
            updateSearchState();
        });
    d->enumerateTimer->setInterval(kSearchIntervals[d->searchState]);
    d->enumerateTimer->start();
    d->initialSearchTimer.start();
}

ManagerWindows::~ManagerWindows()
{
    d->directInput->Release();
}

void ManagerWindows::removeUnpluggedJoysticks(const QSet<QString>& foundDevicePaths)
{
    base_type::removeUnpluggedJoysticks(foundDevicePaths);
    d->foundDevices.clear();
}

DeviceWindowsPtr ManagerWindows::createDevice(
    const JoystickDescriptor& deviceConfig,
    const QString& path,
    LPDIRECTINPUTDEVICE8 directInputDeviceObject)
{
    const auto iter = d->intitializingDevices.find(path);

    if (iter != d->intitializingDevices.end())
    {
        if (!iter.value()->isInitialized())
            return {};

        const auto result = iter.value();
        d->intitializingDevices.erase(iter);
        return result;
    }

    auto device = QSharedPointer<DeviceWindows>(
        new DeviceWindows(directInputDeviceObject, deviceConfig, path, pollTimer(), this));
    connect(device.data(), &Device::failed, this, [this, path] { onDeviceFailed(path); });
    d->intitializingDevices[path] = device;

    return {};
}

void ManagerWindows::enumerateDevices()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    QSet<QString> foundDevicePaths;

    if (!d->foundDevices.isEmpty())
    {
        for (const auto& deviceInfo: d->foundDevices)
        {
            const QString modelName(deviceInfo.model);
            const QString path = deviceInfo.guid;

            NX_VERBOSE(this,
                "A new Joystick has been found. "
                "Model: %1, path: %2",
                modelName, path);

            foundDevicePaths << path;

            if (!m_devices.contains(path))
            {
                const auto config = createDeviceDescription(modelName);

                const auto device = createDevice(
                    config,
                    path,
                    deviceInfo.directInputDeviceObject);
                if (device && device->isValid())
                    initializeDevice(device, config);
                else
                    NX_VERBOSE(this, "Device is invalid. Model: %1, path: %2", modelName, path);
            }
        }
    }

    removeUnpluggedJoysticks(foundDevicePaths);

    d->foundDevices.clear();

    HRESULT status = d->directInput->EnumDevices(
        DI8DEVCLASS_GAMECTRL,
        (LPDIENUMDEVICESCALLBACK)&ManagerWindows::enumDevicesCallback,
        this,
        DIEDFL_ATTACHEDONLY);

    if (status != DI_OK)
        NX_WARNING(this, "Set callback on EnumDevices failed");
}

void ManagerWindows::updateSearchState()
{
    SearchState targetState = d->searchState;
    const bool devicesFound = !devices().empty();

    switch (d->searchState)
    {
        case SearchState::initial:
        {
            if (devicesFound)
                targetState = SearchState::idle;
            else if (!d->initialSearchTimer.isValid())
                d->initialSearchTimer.start();
            else if (d->initialSearchTimer.hasExpired(kInitialSearchPeriod.count()))
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

    if (targetState != d->searchState)
    {
        d->searchState = targetState;
        d->initialSearchTimer.invalidate();
        d->enumerateTimer->stop();
        if (d->searchState != SearchState::idle)
        {
            d->enumerateTimer->setInterval(kSearchIntervals[d->searchState]);
            d->enumerateTimer->start();
        }
        NX_VERBOSE(this, "Switch to search state %1", (int)d->searchState);
    }
}

std::pair<QString, QString> ManagerWindows::getDeviceModelAndGuid(
    LPDIRECTINPUTDEVICE8 inputDevice) const
{
    DIDEVICEINSTANCE joystickinfo;
    joystickinfo.dwSize = sizeof(joystickinfo);

    HRESULT status = inputDevice->GetDeviceInfo(&joystickinfo);
    if (status != DI_OK)
    {
        NX_WARNING(this, "GetDeviceInfo failed");
        return {};
    }

    const auto modelName = QString::fromWCharArray(joystickinfo.tszProductName);
    const auto guid = QString("%1").arg(joystickinfo.guidProduct.Data1, 8, 16, QLatin1Char('0'));

    return {modelName, guid};
}

bool ManagerWindows::enumDevicesCallback(LPCDIDEVICEINSTANCE deviceInstance, LPVOID managerPtr)
{
    if (!deviceInstance)
        return DIENUM_STOP;

    auto manager = reinterpret_cast<ManagerWindows*>(managerPtr);

    LPDIRECTINPUTDEVICE8 inputDevice;

    HRESULT status = manager->d->directInput->CreateDevice(
        deviceInstance->guidInstance,
        &inputDevice,
        nullptr);

    if (status != DI_OK)
    {
        NX_WARNING(manager, "CreateDevice failed: %1", errorCodeToString(status));
        return DIENUM_CONTINUE;
    }

    status = inputDevice->SetDataFormat(&c_dfDIJoystick2);
    if (status != DI_OK)
    {
        inputDevice->Release();
        NX_WARNING(
            manager,
            "Failed to set data format for joystick: %1",
            errorCodeToString(status));
        return DIENUM_CONTINUE;
    }

    status = inputDevice->Acquire();
    if (status != DI_OK)
    {
        inputDevice->Release();
        NX_WARNING(
            manager,
            "Failed to acquire access for joystick: %1",
            errorCodeToString(status));
        return DIENUM_CONTINUE;
    }

    const auto modelAndGuid = manager->getDeviceModelAndGuid(inputDevice);

    const QString& modelName = modelAndGuid.first;
    const QString& guid = modelAndGuid.second;

    if (modelName.isEmpty() && guid.isEmpty())
    {
        NX_VERBOSE(
            manager,
            "Empty device model and guid",
            errorCodeToString(status));

        inputDevice->Release();
        return DIENUM_CONTINUE;
    }

    manager->d->foundDevices.append({inputDevice, modelName, guid});

    return DIENUM_CONTINUE;
}

void ManagerWindows::onDeviceFailed(const QString& path)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    // Device can already be removed, but we still getting callbacks from winapi.
    if (!m_devices.contains(path))
        return;

    QSet<QString> stillActiveDevices = nx::utils::toQSet(m_devices.keys());
    stillActiveDevices.remove(path);
    removeUnpluggedJoysticks(stillActiveDevices);
}

} // namespace nx::vms::client::desktop::joystick
