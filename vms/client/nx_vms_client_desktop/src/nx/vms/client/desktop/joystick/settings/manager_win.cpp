// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "manager_win.h"

#include <libloaderapi.h>

#include <QtCore/QDir>

#include <nx/utils/log/log_main.h>
#include <nx/utils/qset.h>

#include "device_win.h"
#include "descriptors.h"

namespace nx::vms::client::desktop::joystick {

namespace {

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

ManagerWindows::ManagerWindows(QObject* parent):
    base_type(parent)
{
    HRESULT status = DirectInput8Create(
        GetModuleHandle(nullptr),
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        reinterpret_cast<LPVOID*>(&m_directInput),
        nullptr);

    if (status != DI_OK)
        NX_WARNING(this, "DirectInput8Create failed: %1", errorCodeToString(status));
}

ManagerWindows::~ManagerWindows()
{
    m_directInput->Release();
}

void ManagerWindows::removeUnpluggedJoysticks(const QSet<QString>& foundDevicePaths)
{
    base_type::removeUnpluggedJoysticks(foundDevicePaths);
    m_foundDevices.clear();
}

DevicePtr ManagerWindows::createDevice(
    const JoystickDescriptor& deviceConfig,
    const QString& path,
    LPDIRECTINPUTDEVICE8 directInputDeviceObject)
{
    const auto iter = m_intitializingDevices.find(path);

    if (iter != m_intitializingDevices.end())
    {
        if (!iter.value()->isInitialized())
            return {};

        const auto result = iter.value();
        m_intitializingDevices.erase(iter);
        return result;
    }

    auto device = QSharedPointer<DeviceWindows>(
        new DeviceWindows(directInputDeviceObject, deviceConfig, path, pollTimer()));
    connect(device.data(), &Device::failed, this, [this, path] { onDeviceFailed(path); });
    m_intitializingDevices[path] = device;

    return {};
}

void ManagerWindows::enumerateDevices()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    QSet<QString> foundDevicePaths;

    if (!m_foundDevices.isEmpty())
    {
        for (const auto& deviceInfo: m_foundDevices)
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
                const auto iter = std::find_if(m_deviceConfigs.begin(), m_deviceConfigs.end(),
                    [modelName](const JoystickDescriptor& description)
                    {
                        return modelName.contains(description.model);
                    });

                if (iter != m_deviceConfigs.end())
                {
                    const auto config = *iter;

                    auto device = createDevice(
                        config,
                        path,
                        deviceInfo.directInputDeviceObject);

                    if (device && device->isValid())
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
        }
    }

    removeUnpluggedJoysticks(foundDevicePaths);

    m_foundDevices.clear();

    HRESULT status = m_directInput->EnumDevices(
        DI8DEVCLASS_GAMECTRL,
        (LPDIENUMDEVICESCALLBACK)&ManagerWindows::enumDevicesCallback,
        this,
        DIEDFL_ATTACHEDONLY);

    if (status != DI_OK)
        NX_WARNING(this, "Set callback on EnumDevices failed");
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

    HRESULT status = manager->m_directInput->CreateDevice(
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

    manager->m_foundDevices.append({inputDevice, modelName, guid});

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
