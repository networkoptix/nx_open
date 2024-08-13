// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "os_hid_device_mac.h"

#include <QtCore/QTimer>

#include <hidapi/hidapi.h>

#include <nx/utils/log/log.h>

using namespace std::chrono;

namespace {

constexpr milliseconds kDevicePollingInterval = 100ms;

constexpr int kMaxHidReportSize = 1024;

} // namespace

namespace nx::vms::client::desktop::joystick {

struct OsHidDeviceMac::Private
{
    hid_device* dev = nullptr;
    QTimer* pollingTimer;
    JoystickDeviceInfo deviceInfo;
    QBitArray state;
};

OsHidDeviceMac::OsHidDeviceMac(const JoystickDeviceInfo& info):
    d(new Private{.deviceInfo = info})
{
}

OsHidDeviceMac::~OsHidDeviceMac()
{
    stall();
}

JoystickDeviceInfo OsHidDeviceMac::info() const
{
    return d->deviceInfo;
}

bool OsHidDeviceMac::isOpened() const
{
    return d->dev;
}

bool OsHidDeviceMac::open()
{
    d->dev = hid_open_path(d->deviceInfo.path.toStdString().c_str());

    if (!d->dev)
        return false;

    int res = hid_set_nonblocking(d->dev, 1);

    if (res != 0)
    {
        hid_close(d->dev);
        d->dev = nullptr;
        return false;
    }

    if (!d->pollingTimer)
        d->pollingTimer = new QTimer(this);
    d->pollingTimer->setInterval(kDevicePollingInterval);
    connect(d->pollingTimer, &QTimer::timeout, this, &OsHidDeviceMac::poll);
    d->pollingTimer->start();

    return true;
}

int OsHidDeviceMac::read(unsigned char* buffer, int bufferSize)
{
    if (!isOpened())
        return -1;

    return hid_read(d->dev, buffer, bufferSize);
}

void OsHidDeviceMac::poll()
{
    NX_TRACE(this, "Polling device %1...", d->deviceInfo.modelName);

    char buffer[kMaxHidReportSize];
    const int bytesRead = read((unsigned char*)buffer, sizeof(buffer));

    if (bytesRead <= 0)
        return;

    const QBitArray bitState = QBitArray::fromBits(buffer, bytesRead * 8);

    if (bitState != d->state)
    {
        d->state = bitState;

        const State newState {
            .rawData = bitState,
        };

        emit stateChanged(newState);
    }
}

void OsHidDeviceMac::stall()
{
    if (!d->dev)
        return;

    d->pollingTimer->stop();
    hid_close(d->dev);
    d->dev = nullptr;
}

void OsHidDeviceMac::halt()
{
    if (!d->dev)
        return;

    d->pollingTimer->stop();
}

void OsHidDeviceMac::resume()
{
    if (!d->dev)
        return;

    d->pollingTimer->start();
}

} // namespace nx::vms::client::desktop::joystick
