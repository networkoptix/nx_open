// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <network/base_system_description.h>
#include <nx/vms/client/desktop/common/flux/flux_state_store.h>
#include <nx/vms/client/desktop/system_logon/logic/connect_actions_handler.h>
#include <nx/vms/client/desktop/workbench/state/workbench_state.h>

namespace nx::vms::client::desktop {

struct MainWindowTitleBarStateReducer;

struct MainWindowTitleBarState
{
    struct SystemData
    {
        QnSystemDescriptionPtr systemDescription;
        LogonData logonData;
        WorkbenchState workbenchState;

        QString name() const { return systemDescription->name(); }
        bool operator==(const SystemData& other) const;
    };

    bool expanded = false;
    bool homeTabActive = false;
    QnUuid currentSystemId;
    int activeSystemTab = -1;
    ConnectActionsHandler::LogicalState connectState =
        ConnectActionsHandler::LogicalState::disconnected;
    QList<SystemData> systems;
    bool systemUpdating = false;

    int findSystemIndex(const QnUuid& systemId) const;
};

class MainWindowTitleBarStateStore: public QObject, public FluxStateStore<MainWindowTitleBarState>
{
    Q_OBJECT
    using State = MainWindowTitleBarState;
    using Reducer = MainWindowTitleBarStateReducer;

public:
    MainWindowTitleBarStateStore();
    virtual ~MainWindowTitleBarStateStore() override;

    void setExpanded(bool value);
    void setHomeTabActive(bool value);
    void setCurrentSystemId(QnUuid value);
    void setActiveSystemTab(int value);
    void setConnectionState(ConnectActionsHandler::LogicalState value);

    void addSystem(const QnSystemDescriptionPtr& systemDescription, const LogonData& logonData);
    void insertSystem(int index, const State::SystemData systemData);
    void removeSystem(const QnSystemDescriptionPtr& systemDescription);
    void removeSystem(const QnUuid& systemId);
    void removeSystem(int index);
    void removeCurrentSystem();
    void changeCurrentSystem(QnSystemDescriptionPtr systemDescription);
    void moveSystem(int indexFrom, int indexTo);
    void activateHomeTab();
    void setSystemUpdating(bool value);

    int systemCount() const;
    std::optional<State::SystemData> systemData(int index) const;
    bool isExpanded() const;
    bool isHomeTabActive() const;
    bool isLayoutPanelHidden() const;
    int activeSystemTab() const;
    QnUuid currentSystemId() const;
    WorkbenchState workbenchState(const QnUuid& systemId) const;
    bool isTitleBarEnabled() const;

signals:
    void stateChanged(const State& state);
    void systemNameChanged(int index, const QString& name);
};

} // nx::vms::client::desktop
