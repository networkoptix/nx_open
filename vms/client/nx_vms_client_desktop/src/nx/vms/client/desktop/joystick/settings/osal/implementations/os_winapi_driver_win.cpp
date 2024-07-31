// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "os_winapi_driver_win.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <nx/utils/log/log.h>

#include "private/os_winapi_driver_worker_win.h"

using namespace std::chrono;

namespace nx::vms::client::desktop::joystick {

enum class SearchState
{
    /** No joysticks found, for the first time try to find a joystick quickly. If no joysticks
     * found during that period, switch to a slower search (periodic). */
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

struct OsWinApiDriver::Private
{
    QThread* thread = nullptr;
    Worker* poller = nullptr;
    QTimer* const enumerateTimer;
    QElapsedTimer initialSearchTimer;
    SearchState searchState = SearchState::initial;

    void updateSearchState();
    void setSearchState(SearchState newState);
    void resetEnumerateTimer();
};

void OsWinApiDriver::Private::updateSearchState()
{
    SearchState targetState = searchState;
    const bool devicesFound = !poller->devices.empty();

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
            // If joystick was found, stop searching.
            if (devicesFound)
                targetState = SearchState::idle;
            break;
        }
        case SearchState::idle:
        {
            // If joystick was disconnected, switch to quick searching.
            if (!devicesFound)
                targetState = SearchState::initial;
            break;
        }
    }

    if (targetState != searchState)
    {
        initialSearchTimer.invalidate();

        setSearchState(targetState);
    }
}

void OsWinApiDriver::Private::setSearchState(SearchState newState)
{
    if (searchState == newState)
        return;

    searchState = newState;

    NX_VERBOSE(this, "Switch to search state %1", (int)searchState);

    resetEnumerateTimer();
}

void OsWinApiDriver::Private::resetEnumerateTimer()
{
    enumerateTimer->stop();

    if (searchState == SearchState::idle)
        return;

    enumerateTimer->setInterval(kSearchIntervals[searchState]);
    enumerateTimer->start();
}

OsWinApiDriver::OsWinApiDriver():
    d(new Private{
        .enumerateTimer = new QTimer(this)
    })
{
    d->thread = new QThread();
    d->poller = new Worker();
    d->poller->moveToThread(d->thread);

    connect(d->enumerateTimer, &QTimer::timeout, this, &OsWinApiDriver::enumerateDevices);
    connect(d->poller, &Worker::deviceFailed, this, &OsWinApiDriver::enumerateDevices);

    d->thread->start();
    d->enumerateTimer->setInterval(kSearchIntervals[d->searchState]);
    d->enumerateTimer->start();
    d->initialSearchTimer.start();

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

void OsWinApiDriver::halt()
{
    d->setSearchState(SearchState::idle);

    QMetaObject::invokeMethod(d->poller, &Worker::stopDevicePolling, Qt::QueuedConnection);
}

void OsWinApiDriver::resume()
{
    d->updateSearchState();

    QMetaObject::invokeMethod(d->poller, &Worker::startDevicePolling, Qt::QueuedConnection);
}

void OsWinApiDriver::enumerateDevices()
{
    d->updateSearchState();

    QMetaObject::invokeMethod(d->poller, &Worker::enumerateDevices, Qt::QueuedConnection);
}

} // namespace nx::vms::client::desktop::joystick
