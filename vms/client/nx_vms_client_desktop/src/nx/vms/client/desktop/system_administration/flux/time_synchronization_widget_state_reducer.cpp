// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    const nx::Uuid& primaryTimeServer,
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
        state.primaryServer = nx::Uuid();
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
        state.primaryServer = nx::Uuid();
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
    state.primaryServer = nx::Uuid();
    state.status = State::Status::notSynchronized;
    state.hasChanges = true;
    return {true, std::move(state)};
}

Result TimeSynchronizationWidgetReducer::selectServer(State state, const nx::Uuid& serverId)
{
    if (state.primaryServer == serverId)
        return {false, std::move(state)};

    state.enabled = true;
    state.primaryServer = serverId;
    state.primaryServer = actualPrimaryServer(state).id;
    if (state.primaryServer != serverId)
        state.enabled = false;

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

Result TimeSynchronizationWidgetReducer::removeServer(State state, const nx::Uuid& id)
{
    auto& servers = state.servers;
    auto it = std::find_if(servers.begin(), servers.end(),
        [id](const auto& info) { return info.id == id; });
    if (it != servers.end())
    {
        servers.erase(it);
        if (state.primaryServer == id)
            state.enabled = false;

        state.primaryServer = actualPrimaryServer(state).id;
        state.status = actualStatus(state);
        state.commonTimezoneOffset = state.calcCommonTimezoneOffset();
        return {true, std::move(state)};
    }

    return {false, std::move(state)};
}

Result TimeSynchronizationWidgetReducer::setServerOnline(
    State state,
    const nx::Uuid& serverId,
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
    const nx::Uuid& serverId,
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

    // Primary server isn't set. VMS time should be synchronized with the Internet, if possible.
    if (state.primaryServer.isNull())
    {
        return hasInternet(state)
            ? State::Status::synchronizedWithInternet
            : State::Status::noInternetConnection;
    }

    // Primary server is set but missing (e.g. during disconnect).
    if (std::none_of(
        state.servers.cbegin(),
        state.servers.cend(),
        [id = state.primaryServer](const auto& info) { return info.id == id; }))
    {
        return State::Status::notSynchronized;
    }

    // We have a separate status code for a single-server systems.
    if (state.servers.size() == 1)
    {
        NX_ASSERT(state.servers.first().id == state.primaryServer);
        return State::Status::singleServerLocalTime;
    }

    // Find the primary server's info.
    const auto primaryServer = actualPrimaryServer(state);
    NX_ASSERT(state.primaryServer == primaryServer.id);

    // Check server's status.
    return primaryServer.online
        ? State::Status::synchronizedWithSelectedServer
        : State::Status::selectedServerIsOffline;
}

} // namespace nx::vms::client::desktop
