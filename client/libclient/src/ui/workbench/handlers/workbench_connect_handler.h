#pragma once

#include <QtCore/QObject>

#include <nx/utils/raii_guard.h>
#include <nx_ec/ec_api_fwd.h>
#include <crash_reporter.h>

#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

class QnGraphicsMessageBox;
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
        QnRaiiGuardPtr completionWatcher;

        static ConnectionSettingsPtr create(
            bool storePassword,
            bool autoLogin,
            bool forceRemoveOldConnection,
            const QnRaiiGuardPtr& completionWatcher);
    };

private:
    bool connected() const;

    void showLoginDialog();
    void showWelcomeScreen();

    bool tryToRestoreConnection();

    /** Clear all local data structures. */
    void clearConnection();

    /// @brief Connects to server and stores successful connection data
    /// according to specified settings. If no settings are specified no
    /// connection data will be stored.
    Qn::ConnectionResult connectToServer(const QUrl &appServerUrl,
        const ConnectionSettingsPtr &storeSettings,
        bool silent);

    bool disconnectFromServer(bool force);

    ec2::AbstractECConnectionPtr connection2() const;

private:
    void at_messageProcessor_connectionOpened();
    void at_messageProcessor_connectionClosed();

    void at_connectAction_triggered();
    void at_reconnectAction_triggered();
    void at_disconnectAction_triggered();

    void at_beforeExitAction_triggered();
private:
    bool m_processingConnectAction;
    int m_connectingHandle;

    /** Flag that we should handle new connection. */
    bool m_readyForConnection;
    ec2::CrashReporter m_crashReporter;
};
