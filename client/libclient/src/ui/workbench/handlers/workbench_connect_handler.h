#pragma once

#include <QtCore/QObject>

#include <nx/utils/raii_guard.h>
#include <nx_ec/ec_api_fwd.h>
#include <crash_reporter.h>

#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

class QnGraphicsMessageBox;
class QnReconnectInfoDialog;
class QnReconnectHelper;
struct QnConnectionInfo;

class QnWorkbenchConnectHandler: public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;
public:
    /** Logical state - what user should see. */
    enum class LogicalState
    {
        disconnected,           /*< Client is disconnected. Initial state. */
        connecting,             /*< Trying to connect to some system. */
        reconnecting,           /*< Reconnecting to the current system. */
        connecting_to_target,   /*< Connecting to the predefined server (New Window, Link). */
        installing_updates,     /*< Installing updates to the server. */
        connected               /*< Connected and ready to work. */
    };

    /** Internal state. What is under the hood. */
    enum class PhysicalState
    {
        disconnected,           /*< Disconnected. */
        testing,                /*< Know the server url, waiting for QnConnectionInfo. */
        waiting_peer,           /*< Connection established, waiting for peerFound. */
        waiting_resources,      /*< Peer found, waiting for resources. */
        connected               /*< Connected and ready to work. */
    };

    explicit QnWorkbenchConnectHandler(QObject *parent = 0);
    ~QnWorkbenchConnectHandler();

    struct ConnectionSettings;
    typedef QSharedPointer<ConnectionSettings> ConnectionSettingsPtr;

    struct ConnectionSettings
    {
        bool isConnectionToCloud;
        bool storePassword;
        bool autoLogin;
        bool forceRemoveOldConnection;

        static ConnectionSettingsPtr create(
            bool isConnectionToCloud,
            bool storePassword,
            bool autoLogin,
            bool forceRemoveOldConnection);
    };

private:
    void showLoginDialog();

    bool tryToRestoreConnection();

    /** Clear all local data structures. */
    void clearConnection();

    /// @brief Connects to server and stores successful connection data
    /// according to specified settings. If no settings are specified no
    /// connection data will be stored.
    void connectToServer(
        const QUrl &url,
        const ConnectionSettingsPtr &storeSettings);

    bool disconnectFromServer(bool force);

    void handleConnectReply(
        int handle,
        ec2::ErrorCode errorCode,
        ec2::AbstractECConnectionPtr connection,
        const ConnectionSettingsPtr &storeSettings);

    void processReconnectingReply(
        Qn::ConnectionResult status,
        ec2::AbstractECConnectionPtr connection);

    void establishConnection(
        ec2::AbstractECConnectionPtr connection);

    void storeConnectionRecord(
        const QnConnectionInfo& info,
        const ConnectionSettingsPtr& storeSettings);

    void showWarnMessagesOnce();

    void stopReconnecting();

    void setState(LogicalState logicalValue, PhysicalState physicalValue);
    void setLogicalState(LogicalState value);
    void setPhysicalState(PhysicalState value);

signals:
    void stateChanged(LogicalState logicalValue, PhysicalState physicalValue);

private:
    void at_messageProcessor_connectionOpened();
    void at_messageProcessor_connectionClosed();
    void at_messageProcessor_initialResourcesReceived();

    void at_connectAction_triggered();
    void at_reconnectAction_triggered();
    void at_disconnectAction_triggered();

private:
    int m_connectingHandle;
    LogicalState m_logicalState;
    PhysicalState m_physicalState;

    /** Flag that we should handle new connection. */
    bool m_warnMessagesDisplayed;
    ec2::CrashReporter m_crashReporter;

    QPointer<QnReconnectInfoDialog> m_reconnectDialog;
    QScopedPointer<QnReconnectHelper> m_reconnectHelper;
};
