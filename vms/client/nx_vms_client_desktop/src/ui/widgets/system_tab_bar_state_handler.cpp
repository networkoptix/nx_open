// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_tab_bar_state_handler.h"

#include <QtGui/QAction>

#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/credentials_manager.h>
#include <nx/vms/client/core/system_finder/system_description.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/system_logon/ui/welcome_screen.h>
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

    connect(workbench(),
        &Workbench::systemAboutToBeChanged,
        this,
        &SystemTabBarStateHandler::storeWorkbenchState);

    connect(action(menu::RemoveSystemFromTabBarAction),
        &QAction::triggered,
        this,
        &SystemTabBarStateHandler::at_systemDisconnected);

    connect(action(menu::ConnectAction),
        &QAction::triggered,
        this,
        &SystemTabBarStateHandler::at_connectAction);

    connect(windowContext()->connectActionsHandler(),
        &ConnectActionsHandler::stateChanged,
        this,
        &SystemTabBarStateHandler::at_connectionStateChanged);

    connect(appContext()->cloudStatusWatcher(),
        &core::CloudStatusWatcher::statusChanged,
        this,
        [this](core::CloudStatusWatcher::Status status)
        {
            if (status == core::CloudStatusWatcher::Status::LoggedOut)
                dropCloudSystems();
        });

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

void SystemTabBarStateHandler::connectToSystem(
    const core::SystemDescriptionPtr& system, const LogonData& logonData)
{
    executeLater(
        [this, system, ld = logonData]()
        {
            auto logonData = adjustedLogonData(ld, system->localId());
            if (logonData.credentials.authToken.empty())
            {
                action(menu::ResourcesModeAction)->setChecked(false);
                mainWindow()->welcomeScreen()->openArbitraryTile(system->id());
                m_store->removeSystem(system->localId());
                return;
            }

            menu()->trigger(menu::ConnectAction, menu::Parameters()
                .withArgument(Qn::LogonDataRole, logonData));
        },
        this);
}

void SystemTabBarStateHandler::connectToSystem(
    const MainWindowTitleBarState::SystemData& systemData)
{
    connectToSystem(systemData.systemDescription, systemData.logonData);
}

LogonData SystemTabBarStateHandler::adjustedLogonData(
    const LogonData& source, const Uuid& localId) const
{
    LogonData adjusted = source;

    const auto credentials = core::CredentialsManager::credentials(localId);
    if (adjusted.credentials.authToken.empty() && !credentials.empty())
        adjusted.credentials = credentials[0];

    adjusted.connectScenario = ConnectScenario::connectFromTabBar;
    adjusted.storePassword = !credentials.empty() && !credentials[0].authToken.empty();

    return adjusted;
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
    if (!NX_ASSERT(m_store))
        return;

    m_store->changeCurrentSystem(systemDescription);
    if (!m_storedCredentials.authToken.empty())
    {
        m_store->setCurrentCredentials(m_storedCredentials);
        m_storedCredentials = {};
    }
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
    ConnectActionsHandler::LogicalState logicalState)
{
    if (!NX_ASSERT(m_store))
        return;

    m_store->setHomeTabActive(!action(menu::ResourcesModeAction)->isChecked()
        && windowContext()->connectActionsHandler()->logicalState()
            != ConnectActionsHandler::LogicalState::connecting);

    m_store->setConnectionState(logicalState);
    if (logicalState == ConnectActionsHandler::LogicalState::disconnected)
    {
        storeWorkbenchState();
        m_store->setCurrentSystemId({});
        m_store->setActiveSystemTab(-1);
    }
}

void SystemTabBarStateHandler::dropCloudSystems()
{
    QList<MainWindowTitleBarState::SystemData> systems = m_store->state().systems;
    systems.removeIf(
        [](const auto& system)
        {
            return system.logonData.userType == nx::vms::api::UserType::cloud;
        });
    m_store->setSystems(systems);
}

void SystemTabBarStateHandler::at_connectAction()
{
    const auto parameters = menu()->currentParameters(sender());
    m_storedCredentials = parameters.argument<LogonData>(Qn::LogonDataRole).credentials;
}

void SystemTabBarStateHandler::storeWorkbenchState()
{
    if (!NX_ASSERT(m_store))
        return;

    const auto systemId = m_store->currentSystemId();
    if (!systemId.isNull())
    {
        WorkbenchState workbenchState;
        workbench()->submit(workbenchState);
        m_store->setWorkbenchState(systemId, workbenchState);
    }
}

} // namespace nx::vms::client::desktop
