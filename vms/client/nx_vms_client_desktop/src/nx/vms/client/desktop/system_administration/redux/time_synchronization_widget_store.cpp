#include "time_synchronization_widget_store.h"

#include <nx/vms/client/desktop/common/redux/private_redux_store.h>

#include "time_synchronization_widget_state.h"
#include "time_synchronization_widget_state_reducer.h"

#include <nx/utils/algorithm/index_of.h>

namespace nx::vms::client::desktop {

using State = TimeSynchronizationWidgetState;
using Reducer = TimeSynchronizationWidgetReducer;

struct TimeSynchronizationWidgetStore::Private:
    PrivateReduxStore<TimeSynchronizationWidgetStore, State>
{
    using PrivateReduxStore::PrivateReduxStore;
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

void TimeSynchronizationWidgetStore::setVmsTime(std::chrono::milliseconds value)
{
    d->executeAction([&](State state) { state.vmsTime = value; return state; });
}

void TimeSynchronizationWidgetStore::setTimeOffsets(const TimeOffsetInfoList &offsetList)
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
            return state;
        });
}

} // namespace nx::vms::client::desktop
