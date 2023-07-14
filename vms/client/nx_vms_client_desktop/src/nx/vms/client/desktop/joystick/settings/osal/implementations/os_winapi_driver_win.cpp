// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "os_winapi_driver_win.h"

#include <QtCore/QMetaType>
#include <QtCore/QThread>
#include <QtCore/QTimer>

#include "../osal_device.h"
#include "private/os_winapi_driver_worker_win.h"

namespace nx::vms::client::desktop::joystick {

struct OsWinApiDriver::Private
{
    QThread* thread = nullptr;
    Worker* poller = nullptr;
    QTimer* pollingTimer = nullptr;
};

OsWinApiDriver::OsWinApiDriver():
    d(new Private())
{
    d->thread = new QThread();
    d->poller = new Worker();
    d->poller->moveToThread(d->thread);

    d->pollingTimer = new QTimer();
    d->pollingTimer->setInterval(2000);

    connect(d->pollingTimer, &QTimer::timeout, d->poller, &Worker::enumerateDevices);

    d->thread->start();
    d->pollingTimer->start();

    connect(d->poller, &Worker::deviceListChanged,
        [this]() {
            emit deviceListChanged();
        });
}

OsWinApiDriver::~OsWinApiDriver()
{
    d->thread->quit();
    d->thread->wait();
}

QList<JoystickDeviceInfo> OsWinApiDriver::deviceList()
{
    QList<JoystickDeviceInfo> result;
    const auto devices = d->poller->devices;

    for (auto device: devices)
        result.append(device.info);

    return result;
}

void OsWinApiDriver::setupDeviceListener(const QString& path, const OsalDeviceListener* listener)
{
    d->poller->setupDeviceListener(path, listener);
}

void OsWinApiDriver::removeDeviceListener(const OsalDeviceListener* listener)
{
    d->poller->removeDeviceListener(listener);
}

} // namespace nx::vms::client::desktop::joystick
