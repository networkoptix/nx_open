#include "time_syncronization_widget_store.h"

#include <QtCore/QScopedValueRollback>

#include "time_syncronization_widget_state.h"
#include "time_syncronization_widget_state_reducer.h"

namespace nx::client::desktop {

using State = TimeSynchronizationWidgetState;
using Reducer = TimeSynchronizationWidgetReducer;

struct TimeSynchronizationWidgetStore::Private
{
    TimeSynchronizationWidgetStore* const q;
    bool actionInProgress = false;
    State state;

    explicit Private(TimeSynchronizationWidgetStore* owner):
        q(owner)
    {
    }

    void executeAction(std::function<State(State&&)> reduce)
    {
        // Chained actions are forbidden.
        if (actionInProgress)
            return;

        QScopedValueRollback<bool> guard(actionInProgress, true);
        state = reduce(std::move(state));
        emit q->stateChanged(state);
    }
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

void TimeSynchronizationWidgetStore::setVmsTime(std::chrono::milliseconds value)
{
    d->executeAction([&](State state) { state.vmsTime = value; return state; });
}

} // namespace nx::client::desktop
