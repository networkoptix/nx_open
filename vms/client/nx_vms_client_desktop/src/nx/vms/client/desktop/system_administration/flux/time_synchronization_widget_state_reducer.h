// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "time_synchronization_widget_state.h"

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API TimeSynchronizationWidgetReducer
{
public:
    using State = TimeSynchronizationWidgetState;
    using Result = std::pair<bool, State>;

    static State initialize(
        State state,
        bool isTimeSynchronizationEnabled,
        const nx::Uuid& primaryTimeServer,
        const QList<State::ServerInfo>& servers);

    static Result applyChanges(State state);
    static Result setReadOnly(State state, bool value);
    static Result setSyncTimeWithInternet(State state, bool value);
    static Result disableSync(State state);
    static Result selectServer(State state, const nx::Uuid& serverId);

    static Result addServer(State state, const State::ServerInfo& serverInfo);
    static Result removeServer(State state, const nx::Uuid& id);
    static Result setServerOnline(State state, const nx::Uuid& serverId, bool isOnline);
    static Result setServerHasInternet(State state, const nx::Uuid& serverId, bool hasInternet);

    // Utility methods.
    static State::ServerInfo actualPrimaryServer(const State& state);
    static State::Status actualStatus(const State& state);
};

} // namespace nx::vms::client::desktop
