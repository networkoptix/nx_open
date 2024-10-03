// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_tab_bar_state_handler.h"

#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/credentials_manager.h>
#include <nx/vms/client/core/system_finder/system_description.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/remote_session.h>
#include <nx/vms/client/desktop/system_logon/ui/welcome_screen.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

SystemTabBarStateHandler::SystemTabBarStateHandler(
    const QSharedPointer<Store>& store,
    QObject* parent)
    :
    QnWorkbenchContextAware(parent),
    m_store(store)
{
    NX_CRITICAL(m_store);

    connect(m_store.get(),
        &Store::stateChanged,
        this,
        &SystemTabBarStateHandler::handleStateChanged);

    connect(windowContext()->connectActionsHandler(),
        &ConnectActionsHandler::stateChanged,
        this,
        &SystemTabBarStateHandler::handleWorkbenchStateChange);

    connect(system(),
        &SystemContext::remoteIdChanged,
        this,
        &SystemTabBarStateHandler::handleWorkbenchStateChange);

    connect(action(menu::ResourcesModeAction),
        &QAction::triggered,
        m_store.get(),
        &MainWindowTitleBarStateStore::handleResourceModeActionTriggered);

    connect(action(menu::OpenSessionInNewWindowAction),
        &QAction::triggered,
        this,
        &SystemTabBarStateHandler::handleOpenSystemInNewWindowAction);

    connect(appContext()->systemFinder(),
        &core::SystemFinder::systemDiscovered,
        this,
        &SystemTabBarStateHandler::handleSystemDiscovered);

    connect(appContext()->cloudStatusWatcher(),
        &core::CloudStatusWatcher::statusChanged,
        this,
        [this](core::CloudStatusWatcher::Status status)
        {
            if (status == core::CloudStatusWatcher::Status::LoggedOut)
                m_store->removeCloudSessions();
        });
}

void SystemTabBarStateHandler::connectToSystem(const State::SessionData& sessionData)
{
    if (sessionData.cloudConnection)
    {
        menu()->trigger(menu::ConnectToCloudSystemAction,
            menu::Parameters().withArgument(Qn::CloudSystemConnectDataRole,
                CloudSystemConnectData{sessionData.systemId, ConnectScenario::connectFromTabBar}));
        return;
    }

    const LogonData& logonData = this->logonData(sessionData);

    if (logonData.credentials.authToken.empty())
    {
        action(menu::ResourcesModeAction)->setChecked(false);
        mainWindow()->welcomeScreen()->openArbitraryTile(sessionData.systemId);
        m_store->removeSession(sessionData.sessionId);
        return;
    }

    menu()->trigger(
        menu::ConnectAction, menu::Parameters().withArgument(Qn::LogonDataRole, logonData));
}

LogonData SystemTabBarStateHandler::logonData(const State::SessionData& sessionData) const
{
    LogonData logonData;
    logonData.connectScenario = ConnectScenario::connectFromTabBar;
    logonData.address = sessionData.localAddress;

    logonData.credentials = core::CredentialsManager::credentials(
        sessionData.localId, sessionData.localUser.toStdString()).value_or(
            network::http::Credentials());

    logonData.storePassword = !logonData.credentials.authToken.empty();

    return logonData;
}

void SystemTabBarStateHandler::handleStateChanged(const State& state)
{
    action(menu::ResourcesModeAction)->setChecked(
        !state.homeTabActive && state.layoutNavigationVisible);

    const std::shared_ptr<RemoteSession> session = appContext()->currentSystemContext()->session();

    if (state.activeSessionId)
    {
        if (!session || session->sessionId() != *state.activeSessionId)
        {
            const std::optional<State::SessionData>& sessionData =
                state.findSession(*state.activeSessionId);

            if (NX_ASSERT(sessionData))
                connectToSystem(*sessionData);
        }
    }
    else if (appContext()->mainWindowContext()->connectActionsHandler()->logicalState()
        != ConnectActionsHandler::LogicalState::disconnected)
    {
        if (session)
            session->autoTerminateIfNeeded();

        action(menu::DisconnectAction)->trigger();
    }
}

void SystemTabBarStateHandler::handleOpenSystemInNewWindowAction()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto sessionId = parameters.argument<SessionId>(Qn::SessionIdRole);

    std::optional<State::SessionData> sessionData = m_store->state().findSession(sessionId);
    if (!sessionData)
        return;

    appContext()->clientStateHandler()->createNewWindow(logonData(*sessionData));
}

void SystemTabBarStateHandler::handleSystemDiscovered(
    const core::SystemDescriptionPtr& systemDescription)
{
    connect(systemDescription.get(), &core::SystemDescription::systemNameChanged, this,
        [this, systemDescription]()
        {
            m_store->updateSystemName(systemDescription->id(), systemDescription->name());
        });
}

void SystemTabBarStateHandler::handleWorkbenchStateChange()
{
    m_store->updateFromWorkbench(workbenchContext());
}

} // namespace nx::vms::client::desktop
