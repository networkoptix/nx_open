// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "os_winapi_device_win.h"

#include <memory>

#include <QtCore/QTimer>

#include <nx/utils/log/log.h>

using namespace std::chrono;

namespace {

constexpr milliseconds kDevicePollingInterval = 100ms;

constexpr int kMaxHidReportSize = 1024;

} // namespace

namespace nx::vms::client::desktop::joystick {

struct OsWinApiDeviceWin::Private
{
    Device device;

    QTimer* pollingTimer;

    bool isAxisXInitialized = false;
    bool isAxisYInitialized = false;
    bool isAxisZInitialized = false;

    int minX, maxX;
    int minY, maxY;
    int minZ, maxZ;

    int buttonsNumber = 0;

    QMetaObject::Connection pollingConnection;
};

OsWinApiDeviceWin::OsWinApiDeviceWin(const Device& device, QTimer* pollingTimer):
    d(new Private{.device = device, .pollingTimer = pollingTimer})
{
    if (!NX_ASSERT(d->device.inputDevice))
        return;

    HRESULT status = d->device.inputDevice->EnumObjects(
        (LPDIENUMDEVICEOBJECTSCALLBACK)&OsWinApiDeviceWin::enumerateObjectsCallback,
        this,
        DIDFT_ALL);

    if (status != DI_OK)
        NX_WARNING(this, "Set callback on EnumObjects failed");

    if (!NX_ASSERT(d->pollingTimer))
        return;

    d->pollingConnection = connect(d->pollingTimer, &QTimer::timeout, this,
        &OsWinApiDeviceWin::poll);
}

OsWinApiDeviceWin::~OsWinApiDeviceWin()
{
}

JoystickDeviceInfo OsWinApiDeviceWin::info() const
{
    return d->device.info;
}

void OsWinApiDeviceWin::release()
{
    disconnect(d->pollingConnection);

    if (!NX_ASSERT(d->device.inputDevice))
        return;

    d->device.inputDevice->Release();
}

void OsWinApiDeviceWin::poll()
{
    DIJOYSTATE2 hwState;
    HRESULT status = d->device.inputDevice->GetDeviceState(
        sizeof(hwState),
        reinterpret_cast<LPVOID>(&hwState));

    State state{
        .isAxisXInitialized = d->isAxisXInitialized,
        .isAxisYInitialized = d->isAxisYInitialized,
        .isAxisZInitialized = d->isAxisZInitialized,
        .minX = d->minX,
        .maxX = d->maxX,
        .minY = d->minY,
        .maxY = d->maxY,
        .minZ = d->minZ,
        .maxZ = d->maxZ,
        .buttons = QBitArray(d->buttonsNumber),
    };

    if (status != DI_OK)
    {
        NX_WARNING(this, "Failed to get device state");
        // TODO: emit failed();
        emit stateChanged(state);

        return;
    }

    state.x = hwState.lX;
    state.y = hwState.lY;
    state.z = hwState.lZ;

    for (int i = 0; i < std::min((size_t)d->buttonsNumber, sizeof(hwState.rgbButtons)); i++)
        state.buttons[i] = hwState.rgbButtons[i];

    emit stateChanged(state);
}

bool OsWinApiDeviceWin::enumerateObjectsCallback(
    LPCDIDEVICEOBJECTINSTANCE deviceObject,
    LPVOID devicePtr)
{
    auto device = reinterpret_cast<OsWinApiDeviceWin*>(devicePtr);

    if (!device)
        return DIENUM_STOP;

    if ((deviceObject->dwType & DIDFT_ABSAXIS) != 0)
    {
        DIPROPRANGE range;
        range.diph.dwSize = sizeof(DIPROPRANGE);
        range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        range.diph.dwHow = DIPH_BYID;
        range.diph.dwObj = deviceObject->dwType;

        if (device->d->device.inputDevice->GetProperty(DIPROP_RANGE, &range.diph) == DI_OK)
        {
            if (deviceObject->guidType == GUID_XAxis)
            {
                device->d->minX = range.lMin;
                device->d->maxX = range.lMax;
                device->d->isAxisXInitialized = true;
            }
            else if (deviceObject->guidType == GUID_YAxis)
            {
                device->d->minY = range.lMin;
                device->d->maxY = range.lMax;
                device->d->isAxisYInitialized = true;
            }
            else if (deviceObject->guidType == GUID_ZAxis)
            {
                device->d->minZ = range.lMin;
                device->d->maxZ = range.lMax;
                device->d->isAxisZInitialized = true;
            }
        }
    }
    else if (deviceObject->dwType & DIDFT_BUTTON)
    {
        device->d->buttonsNumber++;
    }

    return DIENUM_CONTINUE;
}

} // namespace nx::vms::client::desktop::joystick
