// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "os_hid_device_mac.h"

#include <QtCore/QTimer>

#include <hidapi/hidapi.h>

using namespace std::chrono;

namespace {

constexpr milliseconds kDevicePollingInterval = 100ms;

constexpr int kMaxHidReportSize = 1024;

} // namespace

namespace nx::vms::client::desktop::joystick {

struct OsHidDeviceMac::Private
{
    hid_device* dev = nullptr;
    QTimer* pollingTimer = nullptr;
    OsHidDeviceInfo deviceInfo;
    QBitArray state;
};

OsHidDeviceMac::OsHidDeviceMac(const OsHidDeviceInfo& info): d(new Private{.deviceInfo = info})
{
    d->dev = hid_open_path(d->deviceInfo.path.toStdString().c_str());

    if (d->dev)
    {
        d->pollingTimer = new QTimer(this);
        d->pollingTimer->setInterval(kDevicePollingInterval);
        connect(d->pollingTimer, &QTimer::timeout, this, &OsHidDeviceMac::poll);
        d->pollingTimer->start();
    }
}

OsHidDeviceMac::~OsHidDeviceMac()
{
    stall();
}

OsHidDeviceInfo OsHidDeviceMac::info() const
{
    return d->deviceInfo;
}

bool OsHidDeviceMac::isValid() const
{
    return d->dev;
}

int OsHidDeviceMac::read(unsigned char* buffer, int bufferSize)
{
    if (!isValid())
        return -1;

    return hid_read(d->dev, buffer, bufferSize);
}

void OsHidDeviceMac::poll()
{
    char buffer[kMaxHidReportSize];
    const int bytesRead = read((unsigned char*)buffer, sizeof(buffer));

    if (bytesRead <= 0)
        return;

    const QBitArray newState(QBitArray::fromBits(buffer, bytesRead * 8));

    if (newState != d->state)
    {
        d->state = newState;
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

} // namespace nx::vms::client::desktop::joystick
