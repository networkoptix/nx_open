// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_tab_bar_state_handler.h"

#include <nx/vms/client/core/system_finder/system_description.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/widgets/main_window.h>
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
}

void SystemTabBarStateHandler::setStateStore(QSharedPointer<Store> store)
{
    disconnect(m_store.get());
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
    if (state.systems != m_storedState.systems)
        emit tabsChanged();

    if (state.activeSystemTab != m_storedState.activeSystemTab)
    {
        emit activeSystemTabChanged(state.activeSystemTab);
        if (const auto systemData = m_store->systemData(state.activeSystemTab))
        {
            const auto localSystemId = systemData->systemDescription->localId();
            if (localSystemId != state.currentSystemId)
            {
                auto logonData = systemData->logonData;
                executeLater(
                    [this, logonData]()
                    {
                        menu()->trigger(menu::ConnectAction,
                            menu::Parameters().withArgument(Qn::LogonDataRole, logonData));
                    },
                    this);
            }
        }
    }

    if (state.homeTabActive != m_storedState.homeTabActive)
    {
        mainWindow()->setWelcomeScreenVisible(state.homeTabActive);
        if (state.homeTabActive)
            emit activeSystemTabChanged(state.systems.count());
        else
            emit activeSystemTabChanged(state.activeSystemTab);
    }

    if (state.currentSystemId != m_storedState.currentSystemId)
    {
        if (state.currentSystemId.isNull()
            && state.connectState == ConnectActionsHandler::LogicalState::connected)
        {
            action(menu::DisconnectMainMenuAction)->trigger();
        }
    }

    m_storedState = state;
}

void SystemTabBarStateHandler::at_currentSystemChanged(
    core::SystemDescriptionPtr systemDescription)
{
    if (m_store)
        m_store->changeCurrentSystem(systemDescription);
}

void SystemTabBarStateHandler::at_systemDisconnected()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto systemId = parameters.argument(Qn::LocalSystemIdRole).value<nx::Uuid>();
    m_store->removeSystem(systemId);
}

void SystemTabBarStateHandler::at_connectionStateChanged(
    ConnectActionsHandler::LogicalState logicalValue)
{
    m_store->setConnectionState(logicalValue);
    if (logicalValue == ConnectActionsHandler::LogicalState::disconnected)
    {
        storeWorkbenchState();
        m_store->setCurrentSystemId({});
    }
}

void SystemTabBarStateHandler::storeWorkbenchState()
{
    const auto systemId = m_store->state().currentSystemId;
    if (!systemId.isNull())
    {
        WorkbenchState workbenchState;
        workbench()->submit(workbenchState);
        m_store->setWorkbenchState(systemId, workbenchState);
    }
}

} // namespace nx::vms::client::desktop
