// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "main_window_title_bar_state.h"

#include <network/system_helpers.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/vms/client/core/network/credentials_manager.h>
#include <nx/vms/client/core/system_finder/system_description.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>

namespace {

const QString kSystemTabBarStateDelegate = "systemTabBar";
const QString kSystemsKey = "systems";

} // namespace

namespace nx::vms::client::desktop {

using core::SystemDescriptionPtr;

//-------------------------------------------------------------------------------------------------
// struct MainWindowTitleBarState

QString MainWindowTitleBarState::SystemData::name() const
{
    return systemDescription->name();
}

bool MainWindowTitleBarState::SystemData::operator==(const SystemData& other) const
{
    // FIXME: #aivashchenko Comparing pointers looks very suspicious.
    return this->systemDescription == other.systemDescription;
}

int MainWindowTitleBarState::findSystemIndex(const nx::Uuid& systemId) const
{
    const auto it = std::find_if(systems.cbegin(), systems.cend(),
        [systemId](const auto system)
        {
            return system.systemDescription->localId() == systemId;
        });

    if (it == systems.cend())
        return -1;

    return std::distance(systems.cbegin(), it);
}

int MainWindowTitleBarState::findSystemIndex(const LogonData& logonData) const
{
    const auto it = std::find_if(systems.cbegin(), systems.cend(),
        [logonData](const auto system)
        {
            return system.logonData == logonData;
        });

    if (it == systems.cend())
        return -1;

    return std::distance(systems.cbegin(), it);
}

//-------------------------------------------------------------------------------------------------
// struct MainWindowTitleBarStateReducer

struct MainWindowTitleBarStateReducer
{
    using State = MainWindowTitleBarState;

    static State setExpanded(State&& state, bool value);
    static State setHomeTabActive(State&& state, bool value);
    static State setCurrentSystemId(State&& state, nx::Uuid value);
    static State setActiveSystemTab(State&& state, int index);
    static State activateHomeTab(State&& state);
    static State setConnectionState(State&& state, ConnectActionsHandler::LogicalState value);
    static State addSystem(State&& state, State::SystemData value);
    static State insertSystem(State&& state, int index, State::SystemData value);
    static State removeSystem(State&& state, int index);
    static State removeSystemId(State&& state, nx::Uuid systemId);
    static State removeSystem(State&& state, const LogonData& logonData);
    static State removeCurrentSystem(State&& state);
    static State changeCurrentSystem(State&& state, SystemDescriptionPtr systemDescription);
    static State moveSystem(State&& state, int indexFrom, int IndexTo);
    static State setSystemUpdating(State&& state, bool value);
    static State setSystems(State&& state, QList<State::SystemData> systems);
    static State setWorkbenchState(State&& state, int index, WorkbenchState workbenchState);
};

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

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::setCurrentSystemId(
    State&& state, nx::Uuid value)
{
    state.currentSystemId = value;
    return state;
}

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::setActiveSystemTab(
    State&& state, int index)
{
    state.activeSystemTab = index;
    return state;
}

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::activateHomeTab(
    State&& state)
{
    state.expanded = true;
    state.homeTabActive = true;
    return state;
}

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::setConnectionState(
    State&& state, ConnectActionsHandler::LogicalState value)
{
    state.connectState = value;
    return state;
}

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::addSystem(State&& state,
    MainWindowTitleBarState::SystemData value)
{
    state.systems.append(value);
    return state;
}

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::insertSystem(State&& state,
    int index,
    MainWindowTitleBarState::SystemData value)
{
    state.systems.insert(index, value);
    return state;
}

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::removeSystem(State&& state,
    int index)
{
    if (index < 0)
        return state;

    state.systems.removeAt(index);
    if (index == state.activeSystemTab)
    {
        state.homeTabActive = true;
        state.activeSystemTab = -1;
        state.currentSystemId = {};
    }
    else if (index < state.activeSystemTab)
    {
        state.activeSystemTab--;
    }
    return state;
}

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::removeSystemId(
    State&& state, nx::Uuid systemId)
{
    return removeSystem(std::move(state), state.findSystemIndex(systemId));
}

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::removeSystem(
    State&& state, const LogonData& logonData)
{
    return removeSystem(std::move(state), state.findSystemIndex(logonData));
}

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::removeCurrentSystem(
    State&& state)
{
    return removeSystem(std::move(state), state.activeSystemTab);
}

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::changeCurrentSystem(
    State&& state, SystemDescriptionPtr systemDescription)
{
    const auto systemId = systemDescription->localId();
    const auto index = state.findSystemIndex(systemId);
    state.currentSystemId = systemId;
    state.activeSystemTab = index;
    state.homeTabActive = false;
    return state;
}

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::moveSystem(
    State&& state, int indexFrom, int indexTo)
{
    if (indexFrom < 0 || indexTo < 0 || indexFrom == indexTo
        || indexFrom >= state.systems.count() || indexTo >= state.systems.count())
    {
        return state;
    }

    state.systems.move(indexFrom, indexTo);

    if (state.activeSystemTab == indexFrom)
        state.activeSystemTab = indexTo;
    else if (indexFrom < state.activeSystemTab && indexTo >= state.activeSystemTab)
        --state.activeSystemTab;
    else if (indexFrom > state.activeSystemTab && indexTo <= state.activeSystemTab)
        ++state.activeSystemTab;

    return state;
}

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::setSystemUpdating(
    State&& state, bool value)
{
    state.systemUpdating = value;
    return state;
}

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::setSystems(
    State&& state, QList<MainWindowTitleBarState::SystemData> systems)
{
    state.systems = std::move(systems);
    return state;
}

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::setWorkbenchState(
    State&& state, int index, WorkbenchState workbenchState)
{
    state.systems[index].workbenchState = workbenchState;
    return state;
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
        const StartupParameters& /*params*/) override
    {
        if (!flags.testFlag(Substate::systemIndependentParameters))
            return false;

        QList<nx::Uuid> systemIds;
        QJson::deserialize(state.value(kSystemsKey), &systemIds);
        QList<State::SystemData> systems;
        for (const auto systemId: systemIds)
        {
            auto system = appContext()->systemFinder()->getSystem(systemId.toSimpleString());
            if (!system)
                continue;

            LogonData logonData;
            const auto servers = system->servers();
            const auto it = std::find_if(servers.cbegin(), servers.cend(),
                [system](const api::ModuleInformation& server)
                {
                    return !server.id.isNull();
                });
            if (it != servers.cend())
            {
                auto url = system->getServerHost(it->id);
                if (url.isEmpty())
                    url = system->getServerHost({});
                logonData.address = nx::network::SocketAddress(
                    url.host(),
                    url.port(helpers::kDefaultConnectionPort));
                logonData.connectScenario = ConnectScenario::autoConnect;
            }

            const auto credentials = core::CredentialsManager::credentials(systemId);
            if (!credentials.empty())
                logonData.credentials = credentials[0];

            systems << State::SystemData{
                .systemDescription = system,
                .logonData = logonData};
        }
        m_store->setSystems(std::move(systems));
        return true;
    }

    virtual void saveState(DelegateState* state, SubstateFlags flags) override
    {
        if (flags.testFlag(Substate::systemIndependentParameters))
        {
            DelegateState result;
            QJson::serialize(m_store->systemsIds(), &result[kSystemsKey]);
            *state = result;
        }
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

void MainWindowTitleBarStateStore::setExpanded(bool value)
{
    if (state().expanded != value)
        dispatch(Reducer::setExpanded, value);
}

void MainWindowTitleBarStateStore::setHomeTabActive(bool value)
{
    if (state().homeTabActive != value)
        dispatch(Reducer::setHomeTabActive, value);
}

void MainWindowTitleBarStateStore::setCurrentSystemId(nx::Uuid value)
{
    if (state().currentSystemId != value)
        dispatch(Reducer::setCurrentSystemId, value);
}

void MainWindowTitleBarStateStore::setActiveSystemTab(int value)
{
    dispatch(Reducer::setActiveSystemTab, value);
}

void MainWindowTitleBarStateStore::setConnectionState(ConnectActionsHandler::LogicalState value)
{
    if (state().connectState != value)
        dispatch(Reducer::setConnectionState, value);
}

void MainWindowTitleBarStateStore::addSystem(const SystemDescriptionPtr& systemDescription,
    const LogonData& logonData)
{
    if (state().findSystemIndex(systemDescription->localId()) >= 0)
        return;

    State::SystemData value =
        {.systemDescription = systemDescription, .logonData = logonData};
    dispatch(Reducer::addSystem, value);
}

void MainWindowTitleBarStateStore::insertSystem(int index, const State::SystemData systemData)
{
    dispatch(Reducer::insertSystem, index, systemData);
}

void MainWindowTitleBarStateStore::removeSystem(const SystemDescriptionPtr& systemDescription)
{
    if (systemDescription)
        removeSystem(systemDescription->localId());
}

void MainWindowTitleBarStateStore::removeSystem(const nx::Uuid& systemId)
{
    dispatch(Reducer::removeSystemId, systemId);
}

void MainWindowTitleBarStateStore::removeSystem(int index)
{
    if (index >= 0)
    {
        State (*f)(State&&, int) = &Reducer::removeSystem;
        dispatch(f, index);
    }
}

void MainWindowTitleBarStateStore::removeSystem(const LogonData& logonData)
{
    State (*f)(State&&, const LogonData&) = &Reducer::removeSystem;
    dispatch(f, logonData);
}

void MainWindowTitleBarStateStore::removeCurrentSystem()
{
    dispatch(Reducer::removeCurrentSystem);
}

void MainWindowTitleBarStateStore::changeCurrentSystem(SystemDescriptionPtr systemDescription)
{
    dispatch(Reducer::changeCurrentSystem, systemDescription);
}

void MainWindowTitleBarStateStore::moveSystem(int indexFrom, int indexTo)
{
    dispatch(Reducer::moveSystem, indexFrom, indexTo);
}

void MainWindowTitleBarStateStore::activateHomeTab()
{
    dispatch(Reducer::activateHomeTab);
}

void MainWindowTitleBarStateStore::setSystemUpdating(bool value)
{
    dispatch(Reducer::setSystemUpdating, value);
}

void MainWindowTitleBarStateStore::setSystems(QList<MainWindowTitleBarState::SystemData> systems)
{
    dispatch(Reducer::setSystems, systems);
}

void MainWindowTitleBarStateStore::setWorkbenchState(
    const Uuid& systemId, const WorkbenchState& workbenchState)
{
    const int index = state().findSystemIndex(systemId);
    if (index >= 0)
        dispatch(Reducer::setWorkbenchState, index, workbenchState);
}

int MainWindowTitleBarStateStore::systemCount() const
{
    return state().systems.count();
}

std::optional<MainWindowTitleBarState::SystemData> MainWindowTitleBarStateStore::systemData(
    int index) const
{
    if (index < 0 || index >= systemCount())
        return std::nullopt;

    return state().systems.at(index);
}

bool MainWindowTitleBarStateStore::isExpanded() const
{
    return state().expanded;
}

bool MainWindowTitleBarStateStore::isHomeTabActive() const
{
    return state().homeTabActive;
}

bool MainWindowTitleBarStateStore::isLayoutPanelHidden() const
{
    return state().homeTabActive
        || state().connectState != ConnectActionsHandler::LogicalState::connected;
}

int MainWindowTitleBarStateStore::activeSystemTab() const
{
    return state().activeSystemTab;
}

nx::Uuid MainWindowTitleBarStateStore::currentSystemId() const
{
    return state().currentSystemId;
}

WorkbenchState MainWindowTitleBarStateStore::workbenchState(const nx::Uuid& systemId) const
{
    const int index = state().findSystemIndex(systemId);
    if (index >= 0)
        return state().systems[index].workbenchState;

    return {};
}

bool MainWindowTitleBarStateStore::isTitleBarEnabled() const
{
    return !state().systemUpdating
        && state().connectState != ConnectActionsHandler::LogicalState::connecting;
}

QList<nx::Uuid> MainWindowTitleBarStateStore::systemsIds() const
{
    QList<nx::Uuid> result;
    result.reserve(state().systems.count());
    for (const auto& system: state().systems)
        result.append(system.systemDescription->localId());
    return result;
}

bool MainWindowTitleBarStateStore::hasWorkbenchState() const
{
    return std::any_of(state().systems.cbegin(), state().systems.cend(),
        [](const auto& system)
        {
            return !system.workbenchState.isEmpty();
        });
}

} // nx::vms::client::desktop
