// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "os_hid_device_mac.h"

#include <memory>

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
    std::unique_ptr<QTimer> pollingTimer;
    OsHidDeviceInfo deviceInfo;
    QBitArray state;
};

OsHidDeviceMac::OsHidDeviceMac(const OsHidDeviceInfo& info):
    d(new Private{.deviceInfo = info})
{
}

OsHidDeviceMac::~OsHidDeviceMac()
{
    stall();
}

OsHidDeviceInfo OsHidDeviceMac::info() const
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
        d->pollingTimer = std::make_unique<QTimer>(this);
    d->pollingTimer->setInterval(kDevicePollingInterval);
    connect(d->pollingTimer.get(), &QTimer::timeout, this, &OsHidDeviceMac::poll);
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
