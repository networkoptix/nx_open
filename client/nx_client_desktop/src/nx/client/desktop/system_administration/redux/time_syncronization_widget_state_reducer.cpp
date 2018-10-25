#include "time_syncronization_widget_state_reducer.h"

namespace nx::client::desktop {

using State = TimeSynchronizationWidgetReducer::State;
using Result = TimeSynchronizationWidgetReducer::Result;

State TimeSynchronizationWidgetReducer::initialize(
    State state,
    bool isTimeSynchronizationEnabled,
    const QnUuid& primaryTimeServer,
    const QList<State::ServerInfo>& servers)
{
    state.enabled = isTimeSynchronizationEnabled;
    state.primaryServer = primaryTimeServer;
    state.servers = servers;

    // Actualize primary time server against servers list.
    state.primaryServer = state.actualPrimaryServer();
    state.lastPrimaryServer = state.primaryServer;
    state.hasChanges = false;

    return state;
}

Result TimeSynchronizationWidgetReducer::setReadOnly(
    State state,
    bool value)
{
    if (state.readOnly == value)
        return {false, std::move(state)};

    state.readOnly = value;
    if (value)
        state.hasChanges = false;
    return {true, std::move(state)};
}

Result TimeSynchronizationWidgetReducer::setSyncTimeWithInternet(
    State state,
    bool value)
{
    if (value == state.syncTimeWithInternet())
        return {false, std::move(state)};

    if (value)
    {
        state.enabled = true;
        state.primaryServer = QnUuid();
    }
    else if (state.servers.size() == 1)
    {
        state.enabled = true;
        state.primaryServer = state.servers.first().id;
    }
    else if (!state.lastPrimaryServer.isNull())
    {
        state.enabled = true;
        state.primaryServer = state.lastPrimaryServer;
    }
    else
    {
        state.enabled = false;
        state.primaryServer = QnUuid();
    }
    state.hasChanges = true;
    return {true, std::move(state)};
}

} // namespace nx::client::desktop
