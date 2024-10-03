// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/system_finder/system_description_fwd.h>
#include <ui/widgets/main_window_title_bar_state.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class SystemTabBarStateHandler: public QObject, public QnWorkbenchContextAware
{
    using State = MainWindowTitleBarState;
    using Store = MainWindowTitleBarStateStore;

public:
    SystemTabBarStateHandler(const QSharedPointer<Store>& store, QObject* parent);

private:
    void handleStateChanged(const State& state);
    void handleOpenSystemInNewWindowAction();
    void handleSystemDiscovered(const core::SystemDescriptionPtr& systemDescription);
    void handleWorkbenchStateChange();
    void connectToSystem(const State::SessionData& systemData);
    LogonData logonData(const State::SessionData& systemData) const;

private:
    QSharedPointer<Store> m_store;
    nx::Uuid m_lastActiveSystemId;
};

} // namespace nx::vms::client::desktop
