#pragma once

#include "time_syncronization_widget_state.h"

namespace nx::client::desktop {

class TimeSynchronizationWidgetReducer
{
public:
    using State = TimeSynchronizationWidgetState;
    using Result = std::pair<bool, State>;

    static State initialize(
        State state,
        bool isTimeSynchronizationEnabled,
        const QnUuid& primaryTimeServer,
        const QList<State::ServerInfo>& servers);

    static Result setReadOnly(State state, bool value);
    static Result setSyncTimeWithInternet(State state, bool value);
    static Result disableSync(State state);
    static Result selectServer(State state, const QnUuid& serverId);

    static Result addServer(State state, const State::ServerInfo& serverInfo);
    static Result removeServer(State state, const QnUuid& id);
    static Result setServerOnline(State state, const QnUuid& serverId, bool isOnline);

    // Utility methods.
    static State::ServerInfo actualPrimaryServer(const State& state);
    static State::Status actualStatus(const State& state);
};

} // namespace nx::client::desktop
