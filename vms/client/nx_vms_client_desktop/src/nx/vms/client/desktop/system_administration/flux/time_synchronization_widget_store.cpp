// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_synchronization_widget_store.h"

#include <nx/utils/algorithm/index_of.h>
#include <nx/vms/client/desktop/common/flux/private_flux_store.h>

#include "time_synchronization_widget_state.h"
#include "time_synchronization_widget_state_reducer.h"

namespace nx::vms::client::desktop {

using State = TimeSynchronizationWidgetState;
using Reducer = TimeSynchronizationWidgetReducer;

struct TimeSynchronizationWidgetStore::Private:
    PrivateFluxStore<TimeSynchronizationWidgetStore, State>
{
    using PrivateFluxStore::PrivateFluxStore;
};

TimeSynchronizationWidgetStore::TimeSynchronizationWidgetStore(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

TimeSynchronizationWidgetStore::~TimeSynchronizationWidgetStore()
{
}

const TimeSynchronizationWidgetState& TimeSynchronizationWidgetStore::state() const
{
    return d->state;
}

void TimeSynchronizationWidgetStore::initialize(
    bool isTimeSynchronizationEnabled,
    const QnUuid& primaryTimeServer,
    const QList<State::ServerInfo>& servers)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::initialize(
                std::move(state),
                isTimeSynchronizationEnabled,
                primaryTimeServer,
                servers);
        });
}

void TimeSynchronizationWidgetStore::addServer(const State::ServerInfo &serverInfo)
{
    d->executeAction(
        [&](State state) { return Reducer::addServer(std::move(state), serverInfo); });
}

void TimeSynchronizationWidgetStore::removeServer(const QnUuid& id)
{
    d->executeAction(
        [&](State state) { return Reducer::removeServer(std::move(state), id); });
}

void TimeSynchronizationWidgetStore::setServerOnline(const QnUuid &id, bool isOnline)
{
    d->executeAction(
        [&](State state) { return Reducer::setServerOnline(std::move(state), id, isOnline); });
}

void TimeSynchronizationWidgetStore::setServerHasInternet(const QnUuid &id, bool hasInternet)
{
    d->executeAction(
        [&](State state) { return Reducer::setServerHasInternet(std::move(state), id, hasInternet);
    });
}

void TimeSynchronizationWidgetStore::applyChanges()
{
    d->executeAction([](State state) { return Reducer::applyChanges(std::move(state)); });
}

void TimeSynchronizationWidgetStore::setReadOnly(bool value)
{
    d->executeAction([&](State state) { return Reducer::setReadOnly(std::move(state), value); });
}

void TimeSynchronizationWidgetStore::setSyncTimeWithInternet(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setSyncTimeWithInternet(std::move(state), value); });
}

void TimeSynchronizationWidgetStore::disableSync()
{
    d->executeAction(
        [&](State state) { return Reducer::disableSync(std::move(state)); });
}

void TimeSynchronizationWidgetStore::selectServer(const QnUuid& serverId)
{
    d->executeAction(
        [&](State state) { return Reducer::selectServer(std::move(state), serverId); });
}

void TimeSynchronizationWidgetStore::setTimeOffsets(const TimeOffsetInfoList &offsetList, milliseconds baseTime)
{
    d->executeAction(
        [&](State state)
        {
            for (auto& server: state.servers)
            {
                const auto idx = nx::utils::algorithm::index_of(offsetList,
                    [&server](const TimeOffsetInfo& offsetInfo)
                    {
                        return server.id == offsetInfo.serverId;
                    });

                if (idx >= 0)
                {
                    const auto &offsetInfo = offsetList[idx];
                    server.osTimeOffset = offsetInfo.osTimeOffset;
                    server.vmsTimeOffset = offsetInfo.vmsTimeOffset;
                    server.timeZoneOffset = offsetInfo.timeZoneOffset;
                }
            }

            // Update cached value
            state.commonTimezoneOffset = state.calcCommonTimezoneOffset();
            state.baseTime = baseTime;

            return state;
        });
}

void TimeSynchronizationWidgetStore::setBaseTime(milliseconds time)
{
    d->executeAction(
        [&](State state)
        {
            state.baseTime = time;
            state.elapsedTime = 0ms;

            return state;
        });
}

void TimeSynchronizationWidgetStore::setElapsedTime(milliseconds time)
{
    d->executeAction(
        [&](State state)
        {
            state.elapsedTime = time;

            return state;
        });
}

} // namespace nx::vms::client::desktop
