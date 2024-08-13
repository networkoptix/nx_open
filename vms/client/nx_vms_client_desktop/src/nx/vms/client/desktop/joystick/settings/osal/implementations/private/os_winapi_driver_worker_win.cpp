// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "os_winapi_driver_worker_win.h"

#include <chrono>

#include <QtCore/QTimer>

#include <nx/utils/log/log.h>

#include "../os_winapi_device_win.h"

using namespace std::chrono;

namespace {

constexpr milliseconds kJoystickPollInterval = 100ms;

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

std::pair<QString, QString> getDeviceModelAndGuid(LPDIRECTINPUTDEVICE8 inputDevice)
{
    DIDEVICEINSTANCE joystickInfo;
    joystickInfo.dwSize = sizeof(joystickInfo);

    HRESULT status = inputDevice->GetDeviceInfo(&joystickInfo);
    if (status != DI_OK)
    {
        NX_WARNING(
            typeid(nx::vms::client::desktop::joystick::OsWinApiDriver),
            "GetDeviceInfo failed");
        return {};
    }

    const auto modelName = QString::fromWCharArray(joystickInfo.tszProductName);
    const auto guid = QString("%1").arg(joystickInfo.guidProduct.Data1, 8, 16, QLatin1Char('0'));

    return {modelName, guid};
}

} // namespace

namespace nx::vms::client::desktop::joystick {

OsWinApiDriver::Worker::Worker(QObject* parent)
    :
    QObject(parent)
{
    HRESULT status = DirectInput8Create(
        GetModuleHandle(nullptr),
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        reinterpret_cast<LPVOID*>(&directInput),
        nullptr);

    if (status != DI_OK)
    {
        NX_WARNING(this, "DirectInput8Create failed: %1", errorCodeToString(status));
        return;
    }

    isDirectInputInitialized = true;

    devicePollingTimer = new QTimer(this);
    devicePollingTimer->setInterval(kJoystickPollInterval);
    devicePollingTimer->start();
}

OsWinApiDriver::Worker::~Worker()
{
    qDeleteAll(osWinApiDevices);
}

void OsWinApiDriver::Worker::enumerateDevices()
{
    NX_TRACE(this, "Enumeration started");

    foundDevices.clear();

    // This is a blocking system call.
    HRESULT status = directInput->EnumDevices(
        DI8DEVCLASS_GAMECTRL,
        (LPDIENUMDEVICESCALLBACK) &Worker::enumerationCallback,
        this,
        DIEDFL_ATTACHEDONLY);

    if (status != DI_OK)
    {
        NX_WARNING(this, "Enumeration failed with status: %1", errorCodeToString(status));
        return;
    }

    bool isDevicesChanged = false;

    QSet<QString> registeredDeviceIds;

    for (const auto& device: foundDevices)
    {
        if (devices.contains(device.info.id))
            continue;

        NX_DEBUG(this, "Detected new joystick with model=%1 id=%2", device.info.modelName,
                 device.info.id);

        devices[device.info.id] = device;
        registerDevice(device);
        registeredDeviceIds.insert(device.info.id);

        isDevicesChanged = true;
    }

    QSet<QString> deviceIds;

    for (const auto& deviceId: devices.keys())
        deviceIds.insert(deviceId);

    QSet<QString> newDeviceIds;

    for (const auto& device: foundDevices)
        newDeviceIds.insert(device.info.id);

    for (const auto& disappearedDeviceId: deviceIds - newDeviceIds)
    {
        unregisterDeviceById(disappearedDeviceId);

        isDevicesChanged = true;
    }

    for (const auto& device: foundDevices)
    {
        if (registeredDeviceIds.contains(device.info.id))
            continue;

        device.inputDevice->Release();
    }

    if (isDevicesChanged)
        emit deviceListChanged();

    NX_TRACE(this, "Enumeration finished");
}

bool OsWinApiDriver::Worker::enumerationCallback(
    LPCDIDEVICEINSTANCE deviceInstance,
    LPVOID workerPtr)
{
    NX_TRACE(NX_SCOPE_TAG, "%1 called", __func__);

    if (!deviceInstance)
        return DIENUM_STOP;

    auto worker = reinterpret_cast<Worker*>(workerPtr);

    Q_ASSERT(worker);

    LPDIRECTINPUTDEVICE8 inputDevice;

    HRESULT status = worker->directInput->CreateDevice(
        deviceInstance->guidInstance,
        &inputDevice,
        nullptr);

    if (status != DI_OK)
    {
        NX_WARNING(worker, "%1: CreateDevice failed: %2", __func__, errorCodeToString(status));
        return DIENUM_CONTINUE;
    }

    status = inputDevice->SetDataFormat(&c_dfDIJoystick2);
    if (status != DI_OK)
    {
        inputDevice->Release();
        NX_WARNING(worker, "%1: Failed to set data format for joystick: %2", __func__,
            errorCodeToString(status));
        return DIENUM_CONTINUE;
    }

    status = inputDevice->Acquire();
    if (status != DI_OK)
    {
        inputDevice->Release();
        NX_WARNING(worker, "%1: Failed to acquire access for joystick: %2", __func__,
            errorCodeToString(status));
        return DIENUM_CONTINUE;
    }

    const auto modelAndGuid = getDeviceModelAndGuid(inputDevice);

    const QString& modelName = modelAndGuid.first;
    const QString& guid = modelAndGuid.second;

    if (modelName.isEmpty() && guid.isEmpty())
    {
        NX_VERBOSE(worker, "%1: Empty device model and guid", __func__,
            errorCodeToString(status));

        inputDevice->Release();
        return DIENUM_CONTINUE;
    }

    worker->foundDevices.append(
        {
            .inputDevice = inputDevice,
            .info = { .id = guid, .modelName = modelName },
        });

    return DIENUM_CONTINUE;
}

void OsWinApiDriver::Worker::setupDeviceListener(
    const QString& path,
    const OsalDeviceListener* listener)
{
    if (!deviceSubscribers.contains(path))
    {
        deviceSubscribers.insert(path, {{.listener = listener}});
    }
    else
    {
        auto it = std::find_if(
            deviceSubscribers[path].begin(),
            deviceSubscribers[path].end(),
            [listener](const Subscription& subscription)
            {
              return subscription.listener == listener;
            });

        if (it == deviceSubscribers[path].end())
            deviceSubscribers[path].append({{.listener = listener}});
    }

    if (devices.contains(path))
    {
        const auto device = devices[path];
        const auto osWinApiDevice = osWinApiDevices[path];

        for (auto subscription: deviceSubscribers[device.info.id])
        {
            connect(osWinApiDevice, &OsalDevice::stateChanged,
                subscription.listener, &OsalDeviceListener::onStateChanged);
        }
    }
}

void OsWinApiDriver::Worker::removeDeviceListener(const OsalDeviceListener* listener)
{
    for (auto& subscribers: deviceSubscribers)
    {
        const auto& it = std::find_if(subscribers.begin(), subscribers.end(),
            [listener](const Subscription& subscription)
            {
                return subscription.listener == listener;
            });

        if (it != subscribers.end())
            subscribers.erase(it);
    }
}

void OsWinApiDriver::Worker::registerDevice(const OsWinApiDeviceWin::Device& device)
{
    NX_TRACE(this, "Register device with id=%1", device.info.id);

    auto osWinApiDevice = new OsWinApiDeviceWin(device, devicePollingTimer);
    osWinApiDevices[device.info.id] = osWinApiDevice;

    connect(osWinApiDevice, &OsalDevice::failed, this, &Worker::deviceFailed);

    if (!deviceSubscribers.contains(device.info.id))
        return;

    for (auto subscription: deviceSubscribers[device.info.id])
    {
        connect(osWinApiDevice, &OsalDevice::stateChanged,
            subscription.listener, &OsalDeviceListener::onStateChanged);
    }
}

void OsWinApiDriver::Worker::unregisterDeviceById(const QString& deviceId)
{
    NX_TRACE(this, "Unregister device with id=%1", deviceId);

    const auto osWinApiDevice = osWinApiDevices.take(deviceId);

    if (!NX_ASSERT(osWinApiDevice))
        return;

    osWinApiDevice->deleteLater();
}

void OsWinApiDriver::Worker::stopDevicePolling()
{
    devicePollingTimer->stop();
}

void OsWinApiDriver::Worker::startDevicePolling()
{
    devicePollingTimer->start();
}

} // namespace nx::vms::client::desktop::joystick
