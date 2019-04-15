#pragma once

#include <QtCore/QObject>

#include <nx/utils/scope_guard.h>
#include <nx_ec/ec_api_fwd.h>
#include <crash_reporter.h>

#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>
#include <nx/vms/client/desktop/system_update/client_update_tool.h>

class QnGraphicsMessageBox;
class QnReconnectInfoDialog;
struct QnConnectionInfo;

namespace nx::vms::client::core { class ReconnectHelper; }

class QnWorkbenchConnectHandler: public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;
public:
    /** Logical state - what user should see. */
    enum class LogicalState
    {
        disconnected,           /*< Client is disconnected. Initial state. */
        testing,                /*< Testing connection to some system. */
        connecting,             /*< Trying to connect to some system. */
        reconnecting,           /*< Reconnecting to the current system. */
        connecting_to_target,   /*< Connecting to the predefined server (New Window, Link). */
        installing_updates,     /*< Installing updates to the server.*/
        connected               /*< Connected and ready to work. */
    };

    /** Internal state. What is under the hood. */
    enum class PhysicalState
    {
        disconnected,           /*< Disconnected. */
        testing,                /*< Know the server url, waiting for QnConnectionInfo. */
        waiting_peer,           /*< Connection established, waiting for peerFound. */
        waiting_resources,      /*< Peer found, waiting for resources. */
        pulling_updates,        /*< Pulling updates from the mediaservers. We need to get only a list of mediaservers.*/
        connected               /*< Connected and ready to work. */
    };

    explicit QnWorkbenchConnectHandler(QObject *parent = 0);
    ~QnWorkbenchConnectHandler();

    LogicalState logicalState() const;
    PhysicalState physicalState() const;

    void connectToServer(const nx::utils::Url& url);
    void connectToServerForUpdate(const nx::utils::Url& url);

    enum DisconnectFlag
    {
        NoFlags = 0x0,
        Force = 0x1,
        ErrorReason = 0x2,
        ClearAutoLogin = 0x4,
        SessionTimeout = 0x8,
    };
    Q_DECLARE_FLAGS(DisconnectFlags, DisconnectFlag)

    bool disconnectFromServer(DisconnectFlags flags);

private:
    enum ConnectionOption
    {
        StoreSession = 0x1,
        StorePassword = 0x2,
        AutoLogin = 0x4,
    };
    Q_DECLARE_FLAGS(ConnectionOptions, ConnectionOption)

    void showLoginDialog();

    bool tryToRestoreConnection();

    /** Clear all local data structures. */
    void clearConnection();

    /// @brief Connects to server and stores successful connection data
    /// according to specified settings.
    void testConnectionToServer(
        const nx::utils::Url& url,
        ConnectionOptions options,
        bool force);


    void handleTestConnectionReply(
        int handle,
        const nx::utils::Url& url,
        ec2::ErrorCode errorCode,
        const QnConnectionInfo& connectionInfo,
        ConnectionOptions options,
        bool force);

    void handleConnectReply(
        int handle,
        ec2::ErrorCode errorCode,
        ec2::AbstractECConnectionPtr connection);

    void processReconnectingReply(
        Qn::ConnectionResult status,
        ec2::AbstractECConnectionPtr connection);

    void establishConnection(
        ec2::AbstractECConnectionPtr connection);

    void storeConnectionRecord(nx::utils::Url url,
        const QnConnectionInfo& info,
        ConnectionOptions options);

    void showWarnMessagesOnce();

    void stopReconnecting();

    void setState(LogicalState logicalValue, PhysicalState physicalValue);
    void setLogicalState(LogicalState value);
    void setPhysicalState(PhysicalState value);
    void handleStateChanged(LogicalState logicalValue, PhysicalState physicalValue);

    /**
     * Check if there is at least one online server and try to reconnect. If not, try again later.
     */
    void reconnectStep();

signals:
    void stateChanged(LogicalState logicalValue, PhysicalState physicalValue);

private:
    void at_messageProcessor_connectionOpened();
    void at_messageProcessor_connectionClosed();
    void at_messageProcessor_initialResourcesReceived();

    void at_connectAction_triggered();
    void at_connectToCloudSystemAction_triggered();
    void at_reconnectAction_triggered();
    void at_disconnectAction_triggered();

    void showPreloader();

private:
    struct
    {
        int handle = 0;
        nx::utils::Url url;

        void reset() { handle = 0; url = nx::utils::Url(); }
    } m_connecting;

    LogicalState m_logicalState = LogicalState::disconnected;
    PhysicalState m_physicalState = PhysicalState::disconnected;

    /** Flag that we should handle new connection. */
    bool m_warnMessagesDisplayed = false;
    ec2::CrashReporter m_crashReporter;

    QPointer<QnReconnectInfoDialog> m_reconnectDialog;
    QScopedPointer<nx::vms::client::core::ReconnectHelper> m_reconnectHelper;
};

 Q_DECLARE_OPERATORS_FOR_FLAGS(QnWorkbenchConnectHandler::DisconnectFlags)
