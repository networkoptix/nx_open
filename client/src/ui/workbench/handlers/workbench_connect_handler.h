#ifndef WORKBENCH_CONNECT_HANDLER_H
#define WORKBENCH_CONNECT_HANDLER_H

#include <QtCore/QObject>

#include <nx_ec/ec_api_fwd.h>
#include <crash_reporter.h>

#include <ui/workbench/workbench_context_aware.h>

class QnGraphicsMessageBox;
struct QnConnectionInfo;

class QnWorkbenchConnectHandler : public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QObject base_type;

public:
    explicit QnWorkbenchConnectHandler(QObject *parent = 0);
    ~QnWorkbenchConnectHandler();

    struct StoreConnectionSettings;
    typedef QSharedPointer<StoreConnectionSettings> StoreConnectionSettingsPtr;

    struct StoreConnectionSettings
    {
        QString connectionAlias;
        bool storePassword;
        bool autoLogin;

        static StoreConnectionSettingsPtr create(const QString &connectionAlias
            , bool storePassword
            , bool autoLogin);
    };

    /// @brief Connects to server and stores successful connection data 
    /// according to specified settings. If no settings are specified no 
    /// connection data will be stored.

    ec2::ErrorCode connectToServer(const QUrl &appServerUrl
        , const StoreConnectionSettingsPtr &storeSettings
        , bool silent);

    bool disconnectFromServer(bool force);

    ec2::AbstractECConnectionPtr connection2() const;
    
    bool connected() const;

    void hideMessageBox();
    void showLoginDialog();
    void showWelcomeScreen();

    bool tryToRestoreConnection();

    /** Clear all local data structures. */
    void clearConnection();

private:
    void at_messageProcessor_connectionOpened();
    void at_messageProcessor_connectionClosed();

    void at_connectAction_triggered();
    void at_reconnectAction_triggered();
    void at_disconnectAction_triggered();

    void at_beforeExitAction_triggered();
private:
    QnGraphicsMessageBox* m_connectingMessageBox;
    int m_connectingHandle;

    /** Flag that we should handle new connection. */
    bool m_readyForConnection;
    ec2::CrashReporter m_crashReporter;
};

#endif // WORKBENCH_CONNECT_HANDLER_H
