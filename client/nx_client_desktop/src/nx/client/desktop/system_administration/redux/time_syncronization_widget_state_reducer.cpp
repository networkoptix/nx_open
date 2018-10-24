#include "time_syncronization_widget_state_reducer.h"

namespace nx::client::desktop {

TimeSynchronizationWidgetReducer::State TimeSynchronizationWidgetReducer::initialize(
    State state,
    bool isTimeSynchronizationEnabled,
    bool syncWithInternet,
    const QnUuid& primaryTimeServer,
    const QList<State::ServerInfo>& servers)
{
    state.enabled = isTimeSynchronizationEnabled;
    state.syncTimeWithInternet = syncWithInternet;
    state.primaryServer = primaryTimeServer;
    state.hasChanges = false;


    return state;
}

TimeSynchronizationWidgetReducer::State TimeSynchronizationWidgetReducer::setReadOnly(
    State state,
    bool value)
{
    state.readOnly = value;
    if (value)
        state.hasChanges = false;
    return state;
}

TimeSynchronizationWidgetReducer::State TimeSynchronizationWidgetReducer::setSyncTimeWithInternet(
    State state,
    bool value)
{
    if (value != state.syncTimeWithInternet)
    {
        state.syncTimeWithInternet = value;
        state.hasChanges = true;
    }
    return state;
}

} // namespace nx::client::desktop
