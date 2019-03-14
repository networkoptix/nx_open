#include "time_synchronization_widget_state_reducer.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

using State = TimeSynchronizationWidgetReducer::State;
using Result = TimeSynchronizationWidgetReducer::Result;

namespace {

bool hasInternet(const State& state)
{
    return std::any_of(
        state.servers.cbegin(),
        state.servers.cend(),
        [](const auto& info) { return info.online && info.hasInternet; });
}

} // namespace

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

Result TimeSynchronizationWidgetReducer::applyChanges(State state)
{
    NX_ASSERT(!state.readOnly);

    if(state.hasChanges == false)
        return {false, std::move(state)};

    state.hasChanges = false;
    return {true, std::move(state)};
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
        // Overwrite last primary server
        state.lastPrimaryServer = state.primaryServer;
        state.primaryServer = QnUuid();
        state.status = hasInternet(state)
            ? State::Status::synchronizedWithInternet
            : State::Status::noInternetConnection;
    }
    else if (state.servers.size() == 1)
    {
        state.enabled = true;
        state.primaryServer = state.servers.first().id;
        state.status = State::Status::singleServerLocalTime;
    }
    else if (!state.lastPrimaryServer.isNull())
    {
        state.enabled = true;
        state.primaryServer = state.lastPrimaryServer;
        // Actualize primary time server against servers list.
        state.primaryServer = actualPrimaryServer(state).id;
        state.status = actualStatus(state);
    }
    else
    {
        state.enabled = false;
        state.primaryServer = QnUuid();
        state.status = State::Status::notSynchronized;
    }
    state.hasChanges = true;
    return {true, std::move(state)};
}

Result TimeSynchronizationWidgetReducer::disableSync(State state)
{
    if (!state.enabled)
        return {false, std::move(state)};

    state.lastPrimaryServer = state.primaryServer;
    state.enabled = false;
    state.primaryServer = QnUuid();
    state.status = State::Status::notSynchronized;
    state.hasChanges = true;
    return {true, std::move(state)};
}

Result TimeSynchronizationWidgetReducer::selectServer(State state, const QnUuid& serverId)
{
    if (state.primaryServer == serverId)
        return {false, std::move(state)};

    state.enabled = true;
    state.primaryServer = serverId;
    state.primaryServer = actualPrimaryServer(state).id;
    state.status = actualStatus(state);
    state.hasChanges = true;
    return {true, std::move(state)};
}


Result TimeSynchronizationWidgetReducer::addServer(State state, const State::ServerInfo& serverInfo)
{
    const auto& id = serverInfo.id;
    auto& servers = state.servers;
    if (std::find_if(servers.cbegin(), servers.cend(),
        [id](const auto& info) { return info.id == id; }) == servers.cend())
    {
        servers.push_back(serverInfo);
        state.commonTimezoneOffset = state.calcCommonTimezoneOffset();
        return {true, std::move(state)};
    }

    return {false, std::move(state)};
}

Result TimeSynchronizationWidgetReducer::removeServer(State state, const QnUuid& id)
{
    auto& servers = state.servers;
    auto it = std::find_if(servers.begin(), servers.end(),
        [id](const auto& info) { return info.id == id; });
    if (it != servers.end())
    {
        servers.erase(it);
        state.status = actualStatus(state);
        state.commonTimezoneOffset = state.calcCommonTimezoneOffset();
        return {true, std::move(state)};
    }

    return {false, std::move(state)};
}

Result TimeSynchronizationWidgetReducer::setServerOnline(
    State state,
    const QnUuid& serverId,
    bool isOnline)
{
    for (auto& server: state.servers)
    {
        if (server.id == serverId)
        {
            if (server.online != isOnline)
            {
                server.online = isOnline;
                state.status = actualStatus(state);
                state.commonTimezoneOffset = state.calcCommonTimezoneOffset();
                return {true, std::move(state)};
            }

            return {false, std::move(state)};
        }
    }
    return {false, std::move(state)};
}

Result TimeSynchronizationWidgetReducer::setServerHasInternet(
    State state,
    const QnUuid& serverId,
    bool hasInternet)
{
    for (auto& server: state.servers)
    {
        if (server.id == serverId)
        {
            if (server.hasInternet != hasInternet)
            {
                server.hasInternet = hasInternet;
                state.status = actualStatus(state);
                return {true, std::move(state)};
            }

            return {false, std::move(state)};
        }
    }
    return {false, std::move(state)};
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
        return hasInternet(state)
            ? State::Status::synchronizedWithInternet
            : State::Status::noInternetConnection;
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

} // namespace nx::vms::client::desktop
