#pragma once

#include <QtCore/QObject>

#include <nx/utils/raii_guard.h>
#include <nx_ec/ec_api_fwd.h>
#include <crash_reporter.h>

#include <client/client_connection_status.h>

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
    explicit QnWorkbenchConnectHandler(QObject *parent = 0);
    ~QnWorkbenchConnectHandler();

    struct ConnectionSettings;
    typedef QSharedPointer<ConnectionSettings> ConnectionSettingsPtr;

    struct ConnectionSettings
    {
        bool storePassword;
        bool autoLogin;
        bool forceRemoveOldConnection;

        static ConnectionSettingsPtr create(
            bool storePassword,
            bool autoLogin,
            bool forceRemoveOldConnection);
    };

private:
    void showLoginDialog();
    void showWelcomeScreen();

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

private:
    void at_messageProcessor_connectionOpened();
    void at_messageProcessor_connectionClosed();

    void at_connectAction_triggered();
    void at_reconnectAction_triggered();
    void at_disconnectAction_triggered();

private:
    int m_connectingHandle;
    QnClientConnectionStatus m_state;

    /** Flag that we should handle new connection. */
    bool m_warnMessagesDisplayed;
    ec2::CrashReporter m_crashReporter;

    QPointer<QnReconnectInfoDialog> m_reconnectDialog;
    QScopedPointer<QnReconnectHelper> m_reconnectHelper;
};
