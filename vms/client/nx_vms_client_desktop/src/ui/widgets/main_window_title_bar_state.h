// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/desktop/common/flux/flux_state_store.h>

namespace nx::vms::client::desktop {

struct MainWindowTitleBarState
{
    bool expanded = false;
    bool homeTabActive = false;
};

struct MainWindowTitleBarStateReducer
{
    using State = MainWindowTitleBarState;

    static State setExpanded(State&& state, bool value);
    static State setHomeTabActive(State&& state, bool value);
};

class MainWindowTitleBarStateStore: public QObject, public FluxStateStore<MainWindowTitleBarState>
{
    Q_OBJECT
    using State = MainWindowTitleBarState;
    using Reducer = MainWindowTitleBarStateReducer;

public:
    MainWindowTitleBarStateStore();
    virtual ~MainWindowTitleBarStateStore() override;

    void setExpanded(bool value);
    void setHomeTabActive(bool value);

signals:
    void stateChanged(const State& state);
};

} // nx::vms::client::desktop
