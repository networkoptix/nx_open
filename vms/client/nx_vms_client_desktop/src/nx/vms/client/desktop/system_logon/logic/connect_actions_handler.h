// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QObject>

#include <crash_reporter.h>
#include <nx/reflect/enum_string_conversion.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_common.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/network/remote_connection_fwd.h>
#include <nx/vms/client/core/network/remote_connection_factory.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/desktop/system_logon/data/logon_parameters.h>
#include <nx/vms/client/desktop/system_update/client_update_tool.h>
#include <nx_ec/ec_api_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

namespace nx::vms::api { struct ModuleInformation; }

namespace nx::vms::client::desktop {

class ConnectActionsHandler: public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;
    using RemoteConnectionPtr = nx::vms::client::core::RemoteConnectionPtr;
    using RemoteConnectionError = nx::vms::client::core::RemoteConnectionError;
    using RemoteConnectionFactory = nx::vms::client::core::RemoteConnectionFactory;

public:
    /** Logical state - what user should see. */
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(LogicalState,

        /** Client is disconnected. Probably some connection context exists already. */
        disconnected,

        /** Establishing connection. */
        connecting,

        /** Connected and received initial set of resources. */
        connected
    )

    enum DisconnectFlag
    {
        NoFlags = 0x0,
        Force = 1 << 0,
        ClearAutoLogin = 1 << 1,
        /** Disconnecting by pressing Menu->Disconnect. */
        ManualDisconnect = 1 << 2,
        SwitchingServer = 1 << 3
    };
    Q_DECLARE_FLAGS(DisconnectFlags, DisconnectFlag)

public:
    explicit ConnectActionsHandler(QObject *parent = 0);
    ~ConnectActionsHandler();

signals:
    void stateChanged(LogicalState logicalValue, QPrivateSignal);

private:
    enum ConnectionOption
    {
        StoreSession = 1 << 0,
        StorePassword = 1 << 1,
        UpdateSystemWeight = 1 << 2,
        StorePreferredCloudServer = 1 << 3,
    };
    Q_DECLARE_FLAGS(ConnectionOptions, ConnectionOption)

    LogicalState logicalState() const;

    bool disconnectFromServer(DisconnectFlags flags);

    void showLoginDialog();

    /** Clear all local data structures. */
    void clearConnection();

    /// @brief Connects to server and stores successful connection data
    /// according to specified settings.
    void connectToServer(
        core::ConnectionInfo connectionInfo,
        std::optional<QnUuid> expectedServerId,
        ConnectionOptions options,
        std::optional<ConnectScenario> scenario);

    /**
     * Show a dialog depending on the error code. Suggest user to download compatibility package
     * if possible and restart client to this version, or show factory system web page.
     * @param errorCode Connection error code.
     */
    void handleConnectionError(RemoteConnectionError error);

    void establishConnection(
        RemoteConnectionPtr connection,
        bool autoTerminateServerSession = false);

    void storeConnectionRecord(
        core::ConnectionInfo connectionInfo,
        const nx::vms::api::ModuleInformation& info,
        ConnectionOptions options);

    void showWarnMessagesOnce();

    void setState(LogicalState logicalValue);

    void reportServerSelectionFailure();

    void at_connectAction_triggered();
    void at_connectToCloudSystemAction_triggered();
    void at_reconnectAction_triggered();
    void at_disconnectAction_triggered();
    void at_selectCurrentServerAction_triggered();

    /**
     * Handle connection in the videowall or ACS mode.
     */
    void connectToServerInNonDesktopMode(
        const nx::vms::client::desktop::LogonParameters& parameters);

    void updatePreloaderVisibility();

    /** Limit the time UI spends in "Loading..." state. */
    void setupConnectTimeoutTimer(std::chrono::milliseconds timeout);

    /** Automatically switch cloud host to match the system we are connecting to. */
    void switchCloudHostIfNeeded(const QString& remoteCloudHost);

    void hideReconnectDialog();

    struct CloudSystemEndpoint
    {
        QnUuid serverId;
        nx::network::SocketAddress address;
    };
    std::optional<CloudSystemEndpoint> cloudSystemEndpoint(const QString& systemId) const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ConnectActionsHandler::DisconnectFlags)

} // namespace nx::vms::client::desktop
