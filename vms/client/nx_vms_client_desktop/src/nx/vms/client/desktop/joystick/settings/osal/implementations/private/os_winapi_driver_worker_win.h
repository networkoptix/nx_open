// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QString>

#ifndef DIRECTINPUT_VERSION
    #define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>

#include "../../joystick_device_info.h"
#include "../os_winapi_device_win.h"
#include "../os_winapi_driver_win.h"

class QTimer;

namespace nx::vms::client::desktop::joystick {

class OsWinApiDriver::Worker: public QObject
{
    Q_OBJECT

public:
    Worker(QObject* parent = nullptr);
    ~Worker();

public:
    void enumerateDevices();

signals:
    void deviceListChanged();

public:
    void setupDeviceListener(const QString& path, const OsalDeviceListener* listener);
    void removeDeviceListener(const OsalDeviceListener* listener);

private:
    static bool enumDevicesCallback(LPCDIDEVICEINSTANCE deviceInstance, LPVOID workerPtr);

    void registerDevice(const OsWinApiDeviceWin::Device& device);
    void unregisterDeviceById(const QString& deviceId);

public:
    QMap<QString, OsWinApiDeviceWin::Device> devices;
    QList<OsWinApiDeviceWin::Device> foundDevices;

    LPDIRECTINPUT8 directInput = nullptr; //< Actually, it must be single per app.

    struct Subscription
    {
        const OsalDeviceListener* listener;
        bool isConnected = false;
    };

    QMap<QString, QList<Subscription>> deviceSubscribers;
    QMap<QString, OsWinApiDeviceWin*> osWinApiDevices;

    QTimer* devicePollingTimer = nullptr;
};

} // namespace nx::vms::client::desktop::joystick
