// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "main_window_title_bar_state.h"

#include <network/system_description.h>

namespace nx::vms::client::desktop {

bool MainWindowTitleBarState::SystemData::operator==(const SystemData& other) const
{
    return this->systemDescription == other.systemDescription;
}

int MainWindowTitleBarState::findSystemIndex(const QnUuid& systemId) const
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

struct MainWindowTitleBarStateReducer
{
    using State = MainWindowTitleBarState;

    static State setExpanded(State&& state, bool value);
    static State setHomeTabActive(State&& state, bool value);
    static State setCurrentSystemId(State&& state, QnUuid value);
    static State setActiveSystemTab(State&& state, int index);
    static State activateHomeTab(State&& state);
    static State setConnectionState(State&& state, ConnectActionsHandler::LogicalState value);
    static State addSystem(State&& state, State::SystemData value);
    static State insertSystem(State&& state, int index, State::SystemData value);
    static State removeSystem(State&& state, int index);
    static State removeSystemId(State&& state, QnUuid systemId);
    static State removeCurrentSystem(State&& state);
    static State changeCurrentSystem(State&& state, QnSystemDescriptionPtr systemDescription);
    static State moveSystem(State&& state, int indexFrom, int IndexTo);
    static State setSystemUpdating(State&& state, bool value);
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
    State&& state, QnUuid value)
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
    QnUuid systemId)
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

MainWindowTitleBarStateStore::MainWindowTitleBarStateStore()
{
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

void MainWindowTitleBarStateStore::setCurrentSystemId(QnUuid value)
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

void MainWindowTitleBarStateStore::removeSystem(const QnUuid& systemId)
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

QnUuid MainWindowTitleBarStateStore::currentSystemId() const
{
    return state().currentSystemId;
}

WorkbenchState MainWindowTitleBarStateStore::workbenchState(const QnUuid& systemId) const
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

} // nx::vms::client::desktop
