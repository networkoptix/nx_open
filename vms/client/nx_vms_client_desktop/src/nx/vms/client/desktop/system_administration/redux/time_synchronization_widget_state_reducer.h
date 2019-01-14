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
        const QnUuid& primaryTimeServer,
        const QList<State::ServerInfo>& servers);

    static Result applyChanges(State state);
    static Result setReadOnly(State state, bool value);
    static Result setSyncTimeWithInternet(State state, bool value);
    static Result disableSync(State state);
    static Result selectServer(State state, const QnUuid& serverId);

    static Result addServer(State state, const State::ServerInfo& serverInfo);
    static Result removeServer(State state, const QnUuid& id);
    static Result setServerOnline(State state, const QnUuid& serverId, bool isOnline);
    static Result setServerHasInternet(State state, const QnUuid& serverId, bool hasInternet);

    // Utility methods.
    static State::ServerInfo actualPrimaryServer(const State& state);
    static State::Status actualStatus(const State& state);
};

} // namespace nx::vms::client::desktop
