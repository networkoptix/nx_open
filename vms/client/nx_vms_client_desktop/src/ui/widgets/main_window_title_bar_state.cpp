// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "main_window_title_bar_state.h"

#include <finders/systems_finder.h>
#include <network/system_description.h>
#include <network/system_helpers.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/vms/client/core/network/credentials_manager.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>

namespace {

const QString kSystemTabBarStateDelegate = "systemTabBar";
const QString kSystemsKey = "systems";

} // namespace

namespace nx::vms::client::desktop {

//-------------------------------------------------------------------------------------------------
// struct MainWindowTitleBarState

bool MainWindowTitleBarState::SystemData::operator==(const SystemData& other) const
{
    return this->systemDescription == other.systemDescription;
}

int MainWindowTitleBarState::findSystemIndex(const nx::Uuid& systemId) const
{
    auto it = std::find_if(systems.cbegin(), systems.cend(),
        [systemId](const auto value)
        {
            return value.systemDescription->localId() == systemId;
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
    static State removeCurrentSystem(State&& state);
    static State changeCurrentSystem(State&& state, QnSystemDescriptionPtr systemDescription);
    static State moveSystem(State&& state, int indexFrom, int IndexTo);
    static State setSystemUpdating(State&& state, bool value);
    static State setSystems(State&& state, QList<State::SystemData> systems);
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
    if (!value && state.activeSystemTab < 0)
    {
        if (state.systems.count() > 0)
            state.activeSystemTab = 0;
    }
    else
    {
        state.homeTabActive = value;
    }
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

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::removeSystemId(State&& state,
    nx::Uuid systemId)
{
    return removeSystem(std::move(state), state.findSystemIndex(systemId));
}

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::removeCurrentSystem(
    State&& state)
{
    return removeSystem(std::move(state), state.activeSystemTab);
}

MainWindowTitleBarStateReducer::State MainWindowTitleBarStateReducer::changeCurrentSystem(
    State&& state, QnSystemDescriptionPtr systemDescription)
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
    const auto systemData = state.systems.at(indexFrom);
    state = removeSystem(std::move(state), indexFrom);
    state = insertSystem(std::move(state), indexTo, systemData);
    state.activeSystemTab = indexTo;
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
            auto system = appContext()->systemsFinder()->getSystem(systemId.toSimpleString());

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

void MainWindowTitleBarStateStore::addSystem(const QnSystemDescriptionPtr& systemDescription,
    const LogonData& logonData)
{
    if (state().findSystemIndex(systemDescription->localId()) >= 0)
        return;

    State::SystemData value =
        {.systemDescription = systemDescription, .logonData = std::move(logonData)};
    dispatch(Reducer::addSystem, value);

    connect(systemDescription.get(), &QnSystemDescription::systemNameChanged, this,
        [this, systemDescription]
        {
            const int index = state().findSystemIndex(systemDescription->localId());
            emit systemNameChanged(index, systemDescription->name());
        });
}

void MainWindowTitleBarStateStore::insertSystem(int index, const State::SystemData systemData)
{
    dispatch(Reducer::insertSystem, index, systemData);
}

void MainWindowTitleBarStateStore::removeSystem(const QnSystemDescriptionPtr& systemDescription)
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
        dispatch(Reducer::removeSystem, index);
}

void MainWindowTitleBarStateStore::removeCurrentSystem()
{
    dispatch(Reducer::removeCurrentSystem);
}

void MainWindowTitleBarStateStore::changeCurrentSystem(QnSystemDescriptionPtr systemDescription)
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

} // nx::vms::client::desktop
