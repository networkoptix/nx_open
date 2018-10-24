#pragma once

#include "time_syncronization_widget_state.h"

namespace nx::client::desktop {

class TimeSynchronizationWidgetReducer
{
public:
    using State = TimeSynchronizationWidgetState;

    static State initialize(
        State state,
        bool isTimeSynchronizationEnabled,
        bool syncWithInternet,
        const QnUuid& primaryTimeServer,
        const QList<State::ServerInfo>& servers);

    static State setReadOnly(State state, bool value);
    static State setSyncTimeWithInternet(State state, bool value);
};

} // namespace nx::client::desktop
