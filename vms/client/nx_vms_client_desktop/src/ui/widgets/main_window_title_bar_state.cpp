// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "main_window_title_bar_state.h"

namespace nx::vms::client::desktop {

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::setExpanded(
    State&& state, bool value)
{
    state.expanded = value;
    return state;
}

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::setHomeTabActive(
    State&& state, bool value)
{
    state.homeTabActive = value;
    return state;
}

MainWindowTitleBarStateStore::MainWindowTitleBarStateStore()
{
    subscribe([this](const State& state){ emit stateChanged(state); });
}

MainWindowTitleBarStateStore::~MainWindowTitleBarStateStore()
{
    subscribe(nullptr);
}

void MainWindowTitleBarStateStore::setExpanded(bool value)
{
    dispatch(Reducer::setExpanded, value);
}

void MainWindowTitleBarStateStore::setHomeTabActive(bool value)
{
    dispatch(Reducer::setHomeTabActive, value);
}

} // nx::vms::client::desktop
