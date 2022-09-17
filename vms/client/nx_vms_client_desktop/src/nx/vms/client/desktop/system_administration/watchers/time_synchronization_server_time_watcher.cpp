// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_synchronization_server_time_watcher.h"

#include <QtCore/QPointer>
#include <QtCore/QTimer>

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <utils/common/synctime.h>

#include "../flux/time_synchronization_widget_store.h"

namespace nx::vms::client::desktop {

static constexpr int kDelayTickCount = 15;

class TimeSynchronizationServerTimeWatcher::Private:
    public QObject,
    public core::RemoteConnectionAware
{
    TimeSynchronizationServerTimeWatcher* q = nullptr;

public:
    Private(TimeSynchronizationServerTimeWatcher* q, TimeSynchronizationWidgetStore* store) :
        QObject(),
        q(q),
        m_store(store),
        m_currentRequest(rest::Handle())
    {
        NX_ASSERT(store);

        m_timer.setInterval(1000);
        m_timer.setSingleShot(false);
        connect(&m_timer, &QTimer::timeout, this, &Private::tick);

        m_elapsedTimer.restart();
    }

    void tick()
    {
        if (m_tickCount++ < kDelayTickCount) //< In case of refactoring don't lose the ++
            return;

        if (m_currentRequest)
            return;

        if (!connection())
            return;

        const auto callback = nx::utils::guarded(this,
            [this]
            (bool success, rest::Handle handle, rest::MultiServerTimeData data)
            {
                if (m_currentRequest != handle)
                    return;

                m_currentRequest = rest::Handle(); //< Reset.
                if (!success)
                    return;

                const auto syncTime = qnSyncTime->value();
                const auto rtt = m_elapsedTimer.elapsed();
                TimeSynchronizationWidgetStore::TimeOffsetInfoList offsetList;

                for (const auto& record: data.data)
                {
                    TimeSynchronizationWidgetStore::TimeOffsetInfo offsetInfo;
                    offsetInfo.serverId = record.serverId;
                    offsetInfo.osTimeOffset = record.osTime + rtt/2 - syncTime;
                    offsetInfo.vmsTimeOffset = record.vmsTime + rtt/2 - syncTime;
                    offsetInfo.timeZoneOffset = record.timeZoneOffset;
                    offsetList += offsetInfo;
                }

                if (m_store)
                    m_store->setTimeOffsets(offsetList, syncTime - rtt);
            });

        m_tickCount = 0;
        m_elapsedTimer.restart();

        m_currentRequest = connectedServerApi()->getTimeOfServersAsync(callback, thread());
        m_store->setBaseTime(m_store->state().baseTime + m_store->state().elapsedTime);
    }

    void start()
    {
        m_tickCount = kDelayTickCount; //< Fire request right after the start.
        tick();
        m_timer.start();
    }

    void stop()
    {
        m_timer.stop();
    }

    void forceUpdate()
    {
        m_tickCount = kDelayTickCount;
        m_currentRequest = rest::Handle();
    }

    milliseconds elapsedTime() const
    {
        return m_elapsedTimer.elapsed();
    }

private:
    QPointer<TimeSynchronizationWidgetStore> m_store;
    rest::Handle m_currentRequest;
    qint64 m_currentRequestStartTime = 0;
    int m_tickCount = 0;
    QTimer m_timer;
    nx::utils::ElapsedTimer m_elapsedTimer;
};

TimeSynchronizationServerTimeWatcher::TimeSynchronizationServerTimeWatcher(
    TimeSynchronizationWidgetStore* store,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, store))
{
}

TimeSynchronizationServerTimeWatcher::~TimeSynchronizationServerTimeWatcher()
{
}

void TimeSynchronizationServerTimeWatcher::start()
{
    d->start();
}

void TimeSynchronizationServerTimeWatcher::stop()
{
    d->stop();
}

void TimeSynchronizationServerTimeWatcher::forceUpdate()
{
    d->forceUpdate();
}

milliseconds TimeSynchronizationServerTimeWatcher::elapsedTime() const
{
    return d->elapsedTime();
}

} // namespace nx::vms::client::desktop
