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
    state.primaryServer = actualPrimaryServer(state).id;
    state.lastPrimaryServer = state.primaryServer;
    state.hasChanges = false;

    // If invalid server was passed, disable synchronization.
    if (primaryTimeServer != state.primaryServer)
        state.enabled = false;

    state.status = actualStatus(state);

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

State::ServerInfo TimeSynchronizationWidgetReducer::actualPrimaryServer(const State& state)
{
    if (!state.enabled)
        return {};

    const auto& servers = state.servers;
    if (const auto iter = std::find_if(
        servers.cbegin(),
        servers.cend(),
        [id = state.primaryServer](const auto& info) { return info.id == id; });
        iter != servers.cend())
    {
        return *iter;
    }

    return {};
}

State::Status TimeSynchronizationWidgetReducer::actualStatus(const State& state)
{
    if (!state.enabled)
        return State::Status::notSynchronized;

    const auto primaryServer = actualPrimaryServer(state);
    NX_ASSERT(state.primaryServer == primaryServer.id);

    if (state.primaryServer.isNull())
    {
        if (!std::any_of(state.servers.cbegin(), state.servers.cend(),
            [](const auto& info) { return info.hasInternet; }))
        {
            return State::Status::noInternetConnection;
        }

        return State::Status::synchronizedWithInternet;
    }

    if (state.servers.size() == 1)
    {
        NX_ASSERT(state.servers.first().id == state.primaryServer);
        return State::Status::singleServerLocalTime;
    }

    if (!primaryServer.online)
        return State::Status::selectedServerIsOffline;

    return State::Status::synchronizedWithSelectedServer;
}

} // namespace nx::client::desktop
