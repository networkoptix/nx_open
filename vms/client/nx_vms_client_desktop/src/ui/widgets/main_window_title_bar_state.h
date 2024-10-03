// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/core/system_finder/system_description_fwd.h>
#include <nx/vms/client/desktop/common/flux/flux_state_store.h>
#include <nx/vms/client/desktop/state/session_id.h>
#include <nx/vms/client/desktop/system_logon/logic/connect_actions_handler.h>
#include <nx/vms/client/desktop/workbench/state/workbench_state.h>

namespace nx::vms::client::desktop {

struct MainWindowTitleBarStateReducer;

struct MainWindowTitleBarState
{
    struct SessionData
    {
        SessionId sessionId;

        QString systemName;

        bool cloudConnection = false;
        QString systemId;
        nx::Uuid localId;
        nx::network::SocketAddress localAddress;
        QString localUser;

        WorkbenchState workbenchState;

        SessionData() {}
        SessionData(
            const SessionId& sessionId,
            const nx::vms::api::ModuleInformation& moduleInformation,
            const LogonData& logonData);
    };

    /**
     * When true, the title bar is explicitly set to two-level mode. It may still be displayed as
     * one-level widget when two levels make no sense (layout panel is not visible).
     */
    bool expanded = true;

    /**
     * Home tab is a button which is always displayed next to the system tab bar. When it is
     * active, it is expanded and has a "close" button.
     */
    bool homeTabActive = true;

    /** When true, the layout navigation widget is visible. */
    bool layoutNavigationVisible = false;

    /** A list of currently opened sessions on the system tab bar. */
    QList<SessionData> sessions;

    /**
     * The ID of the currently connected session. Usually the corresponding tab on the system tab
     * bar is displayed as active. However in certain cases the tab bar may have no active tab even
     * though `activeSessionId` is not empty. Typically this happens when the home tab is active.
     */
    std::optional<SessionId> activeSessionId;

    int sessionIndex(const SessionId& sessionId) const;
    std::optional<SessionData> findSession(const SessionId& sessionId) const;

    bool hasWorkbenchState() const;
};

class MainWindowTitleBarStateStore: public QObject, public FluxStateStore<MainWindowTitleBarState>
{
    Q_OBJECT
    using State = MainWindowTitleBarState;
    using Reducer = MainWindowTitleBarStateReducer;

public:
    MainWindowTitleBarStateStore();
    virtual ~MainWindowTitleBarStateStore() override;

    void setExpanded(bool expanded);
    void updateFromWorkbench(QnWorkbenchContext* workbenchContext);

    void setHomeTabActive(bool active);
    void setActiveSessionId(const std::optional<SessionId>& sessionId);
    void setActiveTab(int index);

    void handleResourceModeActionTriggered(bool checked);

    void addSession(State::SessionData sessionData);
    void updateSystemName(const QString& systemId, const QString& name);
    void removeSession(const SessionId& sessionId);

    /**
     * Removes the specified session, but evaluates the `confirmationFunction` first when
     * necessary. If the result is `false`, the session is not removed and the state is kept
     * unchanged. The function should ask the user if they want to disconnect.
     */
    void disconnectAndRemoveSession(
        const SessionId& sessionId, const std::function<bool()>& confirmationFunction);

    void removeSessionsBySystemId(const QString& systemId);
    void moveSession(int indexFrom, int indexTo);
    void setSessions(QList<State::SessionData> sessions);
    void removeCloudSessions();
    void saveWorkbenchState(QnWorkbenchContext* workbenchContext);

signals:
    void stateChanged(const State& state);

private:
    class StateDelegate;
};

} // nx::vms::client::desktop
