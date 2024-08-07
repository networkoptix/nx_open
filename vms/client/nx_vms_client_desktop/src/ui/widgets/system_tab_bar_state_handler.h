// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/system_finder/system_description_fwd.h>
#include <ui/widgets/main_window_title_bar_state.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class SystemTabBarStateHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using State = MainWindowTitleBarState;
    using Store = MainWindowTitleBarStateStore;

public:
    SystemTabBarStateHandler(QObject* parent);
    void setStateStore(QSharedPointer<Store> store);

signals:
    void tabsChanged();
    void activeSystemTabChanged(int index);
    void homeTabActiveChanged(bool active);

private:
    void at_stateChanged(const State& state);
    void at_currentSystemChanged(core::SystemDescriptionPtr systemDescription);
    void at_systemDisconnected();
    void at_connectionStateChanged(ConnectActionsHandler::LogicalState logicalValue);
    void storeWorkbenchState();

private:
    QSharedPointer<Store> m_store;
    State m_storedState;
};

} // namespace nx::vms::client::desktop
