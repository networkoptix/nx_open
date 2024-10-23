// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "main_window_title_bar_state.h"

#include <QtGui/QAction>

#include <network/system_helpers.h>
#include <nx/reflect/instrument.h>
#include <nx/vms/client/core/network/credentials_manager.h>
#include <nx/vms/client/core/system_finder/system_description.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/remote_session.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>

namespace {

const QString kSystemTabBarStateDelegate = "systemTabBar";
const QString kSessionsKey = "sessions";

} // namespace

namespace nx::vms::client::desktop {

using core::SystemDescriptionPtr;

using State = MainWindowTitleBarState;

//-------------------------------------------------------------------------------------------------
// struct MainWindowTitleBarState

NX_REFLECTION_INSTRUMENT(MainWindowTitleBarState::SessionData,
    // `workbenchState` should not be serialized.
    (sessionId)(systemName)(cloudConnection)(systemId)(localId)(localAddress)(localUser))

State::SessionData::SessionData(
    const SessionId& sessionId,
    const nx::vms::api::ModuleInformation& moduleInformation,
    const LogonData& logonData)
    :
    sessionId(sessionId),
    systemName(moduleInformation.systemName),
    cloudConnection(logonData.userType == api::UserType::cloud),
    systemId(::helpers::getTargetSystemId(moduleInformation))
{
    if (!cloudConnection)
    {
        localId = moduleInformation.localSystemId;
        localAddress = logonData.address;
        localUser = QString::fromStdString(logonData.credentials.username);
    }
}

int MainWindowTitleBarState::sessionIndex(const SessionId& sessionId) const
{
    for (int i = 0; i < sessions.size(); ++i)
    {
        if (sessions[i].sessionId == sessionId)
            return i;
    }

    return -1;
}

std::optional<State::SessionData> MainWindowTitleBarState::findSession(
    const SessionId& sessionId) const
{
    for (const SessionData& session: sessions)
    {
        if (session.sessionId == sessionId)
            return session;
    }

    return std::nullopt;
}

bool MainWindowTitleBarState::hasWorkbenchState() const
{
    return std::ranges::any_of(sessions,
        [](const SessionData& session) { return !session.workbenchState.isEmpty(); });
}

//-------------------------------------------------------------------------------------------------
// struct MainWindowTitleBarStateReducer

struct MainWindowTitleBarStateReducer
{
    static State setExpanded(State&& state, bool expanded);
    static State setHomeTabActive(State&& state, bool active);
    static State setActiveSessionId(
        State&& state, const std::optional<SessionId>& sessionId);
    static State setActiveTab(State&& state, int index);
    static State handleResourceModeActionToggled(State&& state, bool checked);
    static State addSession(State&& state, State::SessionData sessionData);
    static State updateSystemName(State&& state, const QString& systemId, const QString& name);
    static State removeSession(State&& state, const SessionId& sessionId);
    static State disconnectAndRemoveSession(
        State&& state,
        const SessionId& sessionId,
        const std::function<bool()>& confirmationFunction);
    static State moveSession(State&& state, int indexFrom, int IndexTo);
    static State selectAnotherSessionIfNecessary(State&& state);
    static State setSessions(State&& state, QList<State::SessionData> sessions);
    static State removeCloudSessions(State&& state);
    static State removeSessionsBySystemId(State&& state, const QString& systemId);
    static State saveWorkbenchState(
        State&& state, const SessionId& sessionId, WorkbenchState workbenchState);
    static State updateFromWorkbench(State&& state, QnWorkbenchContext* workbenchContext);
};

State MainWindowTitleBarStateReducer::setExpanded(State&& state, bool expanded)
{
    state.expanded = expanded;
    return state;
}

State MainWindowTitleBarStateReducer::setHomeTabActive(State&& state, bool active)
{
    if (!NX_ASSERT(active || state.activeSessionId))
    {
        // We cannot deactivate home tab when there's no active connection.
        return state;
    }

    state.homeTabActive = active;
    if (state.homeTabActive)
    {
        state.expanded = true;
        state.layoutNavigationVisible = false;
    }
    else
    {
        state.layoutNavigationVisible = state.activeSessionId.has_value();
    }

    if (!active && !state.activeSessionId && !state.sessions.empty())
        return setActiveSessionId(std::move(state), state.sessions.first().sessionId);

    return state;
}

State MainWindowTitleBarStateReducer::setActiveSessionId(
    State&& state, const std::optional<SessionId>& sessionId)
{
    state.activeSessionId = sessionId;
    state.homeTabActive = !sessionId;
    if (!sessionId)
        state.layoutNavigationVisible = false;
    return state;
}

State MainWindowTitleBarStateReducer::setActiveTab(State&& state, int index)
{
    if (!NX_ASSERT(index < state.sessions.size()))
        return state;

    std::optional<SessionId> sessionId;
    if (index >= 0)
        sessionId = state.sessions[index].sessionId;

    return setActiveSessionId(std::move(state), sessionId);
}

State MainWindowTitleBarStateReducer::handleResourceModeActionToggled(State&& state, bool checked)
{
    if (!checked)
        return setHomeTabActive(std::move(state), true);

    // Currently this action can be manually triggered only when there's no active connection and
    // it should open local files.
    if (!NX_ASSERT(!state.activeSessionId))
        return state;

    state.homeTabActive = false;
    state.layoutNavigationVisible = true;

    return state;
}

State MainWindowTitleBarStateReducer::addSession(State&& state, State::SessionData sessionData)
{
    state.sessions.append(std::move(sessionData));
    return state;
}

State MainWindowTitleBarStateReducer::updateSystemName(
    State&& state, const QString& systemId, const QString& name)
{
    // There are may be more than one session with the same systemId.
    for (State::SessionData& system: state.sessions)
    {
        if (system.systemId == systemId)
            system.systemName = name;
    }
    return state;
}

State MainWindowTitleBarStateReducer::removeSession(State&& state, const SessionId& sessionId)
{
    int index = state.sessionIndex(sessionId);
    if (index < 0)
        return state;

    state.sessions.removeAt(index);
    if (sessionId == state.activeSessionId)
    {
        index = qBound(-1, index, state.sessions.size() - 1);
        return setActiveTab(std::move(state), index);
    }
    return state;
}

State MainWindowTitleBarStateReducer::disconnectAndRemoveSession(
    State&& state,
    const SessionId& sessionId,
    const std::function<bool()>& confirmationFunction)
{
    if (state.activeSessionId == sessionId && !confirmationFunction())
        return state;

    return removeSession(std::move(state), sessionId);
}

State MainWindowTitleBarStateReducer::moveSession(State&& state, int indexFrom, int indexTo)
{
    if (indexFrom < 0 || indexTo < 0 || indexFrom == indexTo
        || indexFrom >= state.sessions.count() || indexTo >= state.sessions.count())
    {
        return state;
    }

    state.sessions.move(indexFrom, indexTo);

    return state;
}

State MainWindowTitleBarStateReducer::selectAnotherSessionIfNecessary(State&& state)
{
    if (state.activeSessionId && !state.findSession(*state.activeSessionId))
    {
        if (state.sessions.isEmpty())
            state.activeSessionId = {};
        else
            state.activeSessionId = state.sessions.first().sessionId;
    }
    return state;
}

State MainWindowTitleBarStateReducer::setSessions(
    State&& state, QList<State::SessionData> sessions)
{
    state.sessions = std::move(sessions);
    return selectAnotherSessionIfNecessary(std::move(state));
}

State MainWindowTitleBarStateReducer::removeCloudSessions(State&& state)
{
    state.sessions.removeIf(
        [](const State::SessionData& session) { return session.cloudConnection; });
    return selectAnotherSessionIfNecessary(std::move(state));
}

State MainWindowTitleBarStateReducer::removeSessionsBySystemId(
    State&& state, const QString& systemId)
{
    state.sessions.removeIf(
        [systemId](const State::SessionData& session) { return session.systemId == systemId; });
    return selectAnotherSessionIfNecessary(std::move(state));
}

State MainWindowTitleBarStateReducer::saveWorkbenchState(
    State&& state, const SessionId& sessionId, WorkbenchState workbenchState)
{
    const int sessionIndex = state.sessionIndex(sessionId);
    if (sessionIndex >= 0)
        state.sessions[sessionIndex].workbenchState = workbenchState;
    return state;
}

State MainWindowTitleBarStateReducer::updateFromWorkbench(
    State&& state, QnWorkbenchContext* workbenchContext)
{
    const ConnectActionsHandler::LogicalState connectionState =
        workbenchContext->windowContext()->connectActionsHandler()->logicalState();

    const bool welcomeScreenOpened =
        !workbenchContext->action(menu::ResourcesModeAction)->isChecked();

    state.homeTabActive = welcomeScreenOpened
        && connectionState != ConnectActionsHandler::LogicalState::connecting;

    state.layoutNavigationVisible = !welcomeScreenOpened
        && connectionState == ConnectActionsHandler::LogicalState::connected;

    std::optional<SessionId> sessionId;
    if (connectionState != ConnectActionsHandler::LogicalState::disconnected)
    {
        const std::shared_ptr<RemoteSession> session =
            workbenchContext->systemContext()->session();

        if (session)
            sessionId = session->sessionId();
    }

    return setActiveSessionId(std::move(state), sessionId);
}

//-------------------------------------------------------------------------------------------------
// class MainWindowTitleBarStateStore::StateDelegate

class MainWindowTitleBarStateStore::StateDelegate: public ClientStateDelegate
{
    using base_type = ClientStateDelegate;
    using Store = MainWindowTitleBarStateStore;

public:
    StateDelegate(Store* store): m_store(store)
    {
    }

    virtual bool loadState(
        const DelegateState& state,
        SubstateFlags flags,
        const StartupParameters& params) override
    {
        if (!flags.testFlag(Substate::systemIndependentParameters)
            || params.allowMultipleClientInstances)
        {
            return false;
        }

        const QByteArray serialized =
            QJsonDocument(state.value(kSessionsKey).toArray()).toJson(QJsonDocument::Compact);

        const auto& [sessions, result] =
            reflect::json::deserialize<decltype(State::sessions)>(serialized.toStdString());

        if (!result)
            return false;

        m_store->setSessions(sessions);
        return true;
    }

    virtual void saveState(DelegateState* state, SubstateFlags flags) override
    {
        if (!flags.testFlag(Substate::systemIndependentParameters))
            return;

        const std::string serialized = reflect::json::serialize(m_store->state().sessions);

        DelegateState result;
        result[kSessionsKey] =
            QJsonDocument::fromJson(QByteArray::fromStdString(serialized)).array();
        *state = result;
    }

    virtual void createInheritedState(DelegateState*, SubstateFlags, const QStringList&) override
    {
        // Just return an empty current state without saving and modyfying.
        return;
    }

private:
    QPointer<Store> m_store;
};

//-------------------------------------------------------------------------------------------------
// class MainWindowTitleBarStateStore

MainWindowTitleBarStateStore::MainWindowTitleBarStateStore()
{
    appContext()->clientStateHandler()->registerDelegate(
        kSystemTabBarStateDelegate, std::make_unique<StateDelegate>(this));
    subscribe([this](const State& state)
        {
            emit stateChanged(state);
        });
}

MainWindowTitleBarStateStore::~MainWindowTitleBarStateStore()
{
    subscribe(nullptr);
}

void MainWindowTitleBarStateStore::setExpanded(bool expanded)
{
    if (state().expanded != expanded)
        dispatch(&Reducer::setExpanded, expanded);
}

void MainWindowTitleBarStateStore::updateFromWorkbench(QnWorkbenchContext* workbenchContext)
{
    dispatch(&Reducer::updateFromWorkbench, workbenchContext);
}

void MainWindowTitleBarStateStore::setHomeTabActive(bool active)
{
    if (state().homeTabActive != active)
        dispatch(&Reducer::setHomeTabActive, active);
}

void MainWindowTitleBarStateStore::setActiveSessionId(const std::optional<SessionId>& sessionId)
{
    if (state().activeSessionId != sessionId)
        dispatch(&Reducer::setActiveSessionId, sessionId);
}

void MainWindowTitleBarStateStore::setActiveTab(int index)
{
    dispatch(&Reducer::setActiveTab, index);
}

void MainWindowTitleBarStateStore::handleResourceModeActionTriggered(bool checked)
{
    dispatch(&Reducer::handleResourceModeActionToggled, checked);
}

void MainWindowTitleBarStateStore::addSession(State::SessionData sessionData)
{
    if (!state().findSession(sessionData.sessionId))
        dispatch(&Reducer::addSession, sessionData);
}

void MainWindowTitleBarStateStore::updateSystemName(const QString& systemId, const QString& name)
{
    dispatch(&Reducer::updateSystemName, systemId, name);
}

void MainWindowTitleBarStateStore::removeSession(const SessionId& sessionId)
{
    dispatch(&Reducer::removeSession, sessionId);
}

void MainWindowTitleBarStateStore::disconnectAndRemoveSession(
    const SessionId& sessionId, const std::function<bool()>& confirmationFunction)
{
    dispatch(&Reducer::disconnectAndRemoveSession, sessionId, confirmationFunction);
}

void MainWindowTitleBarStateStore::removeSessionsBySystemId(const QString& systemId)
{
    dispatch(&Reducer::removeSessionsBySystemId, systemId);
}

void MainWindowTitleBarStateStore::moveSession(int indexFrom, int indexTo)
{
    dispatch(&Reducer::moveSession, indexFrom, indexTo);
}

void MainWindowTitleBarStateStore::setSessions(
    QList<MainWindowTitleBarState::SessionData> sessions)
{
    dispatch(&Reducer::setSessions, sessions);
}

void MainWindowTitleBarStateStore::removeCloudSessions()
{
    dispatch(&Reducer::removeCloudSessions);
}

void MainWindowTitleBarStateStore::saveWorkbenchState(QnWorkbenchContext* workbenchContext)
{
    const std::shared_ptr<RemoteSession> session =
        workbenchContext->windowContext()->system()->session();

    if (!session)
        return;

    WorkbenchState workbenchState;
    workbenchContext->workbench()->submit(workbenchState);
    dispatch(&Reducer::saveWorkbenchState, session->sessionId(), workbenchState);
}

} // nx::vms::client::desktop
