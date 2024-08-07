// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_tab_bar_state_handler.h"

#include <QtGui/QAction>

#include <nx/vms/client/core/system_finder/system_description.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

SystemTabBarStateHandler::SystemTabBarStateHandler(QObject* parent):
    QnWorkbenchContextAware(parent)
{
    connect(workbench(),
        &Workbench::currentSystemChanged,
        this,
        &SystemTabBarStateHandler::at_currentSystemChanged);
    connect(action(menu::RemoveSystemFromTabBarAction),
        &QAction::triggered,
        this,
        &SystemTabBarStateHandler::at_systemDisconnected);
    connect(windowContext()->connectActionsHandler(),
        &ConnectActionsHandler::stateChanged,
        this,
        &SystemTabBarStateHandler::at_connectionStateChanged);
    connect(windowContext(),
        &WindowContext::beforeSystemChanged,
        this,
        &SystemTabBarStateHandler::storeWorkbenchState);

    connect(action(menu::ResourcesModeAction), &QAction::toggled, this,
        [this](bool value)
        {
            if (!m_store)
                return;

            const auto connectionState =
                windowContext()->connectActionsHandler()->logicalState();

            m_store->setHomeTabActive(!value
                && connectionState != ConnectActionsHandler::LogicalState::connecting);

            m_store->setExpanded(!value
                || connectionState != ConnectActionsHandler::LogicalState::disconnected);
        });
}

void SystemTabBarStateHandler::setStateStore(QSharedPointer<Store> store)
{
    if (m_store)
        m_store->disconnect(this);

    m_store = store;

    if (m_store)
    {
        connect(m_store.get(),
            &MainWindowTitleBarStateStore::stateChanged,
            this,
            &SystemTabBarStateHandler::at_stateChanged);
    }
}

void SystemTabBarStateHandler::at_stateChanged(const State& state)
{
    if (!NX_ASSERT(m_store))
        return;

    if (state.systems != m_storedState.systems)
        emit tabsChanged();

    if (state.homeTabActive != m_storedState.homeTabActive)
    {
        emit homeTabActiveChanged(state.homeTabActive);
    }
    else if (state.activeSystemTab != m_storedState.activeSystemTab && !state.homeTabActive)
    {
        emit activeSystemTabChanged(state.activeSystemTab);
    }

    m_storedState = state;
}

void SystemTabBarStateHandler::at_currentSystemChanged(
    core::SystemDescriptionPtr systemDescription)
{
    if (NX_ASSERT(m_store))
        m_store->changeCurrentSystem(systemDescription);
}

void SystemTabBarStateHandler::at_systemDisconnected()
{
    if (!NX_ASSERT(m_store))
        return;

    const auto parameters = menu()->currentParameters(sender());
    const auto systemId = parameters.argument(Qn::LocalSystemIdRole).value<nx::Uuid>();
    m_store->removeSystem(systemId);
}

void SystemTabBarStateHandler::at_connectionStateChanged(
    ConnectActionsHandler::LogicalState logicalValue)
{
    if (!NX_ASSERT(m_store))
        return;

    m_store->setHomeTabActive(!action(menu::ResourcesModeAction)->isChecked()
        && windowContext()->connectActionsHandler()->logicalState()
            != ConnectActionsHandler::LogicalState::connecting);

    m_store->setConnectionState(logicalValue);
    if (logicalValue == ConnectActionsHandler::LogicalState::disconnected)
    {
        storeWorkbenchState();
        m_store->setCurrentSystemId({});
    }
}

void SystemTabBarStateHandler::storeWorkbenchState()
{
    if (!NX_ASSERT(m_store))
        return;

    const auto systemId = m_store->state().currentSystemId;
    if (!systemId.isNull())
    {
        WorkbenchState workbenchState;
        workbench()->submit(workbenchState);
        m_store->setWorkbenchState(systemId, workbenchState);
    }
}

} // namespace nx::vms::client::desktop
