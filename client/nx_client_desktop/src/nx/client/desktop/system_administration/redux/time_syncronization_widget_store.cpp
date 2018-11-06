#include "time_syncronization_widget_store.h"

#include <nx/client/desktop/common/redux/private_redux_store.h>

#include "time_syncronization_widget_state.h"
#include "time_syncronization_widget_state_reducer.h"

namespace nx::client::desktop {

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

} // namespace nx::client::desktop
