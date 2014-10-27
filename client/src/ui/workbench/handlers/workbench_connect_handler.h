#ifndef WORKBENCH_CONNECT_HANDLER_H
#define WORKBENCH_CONNECT_HANDLER_H

#include <QtCore/QObject>

#include <nx_ec/ec_api_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

class QnGraphicsMessageBox;
class QnLoginDialog;
struct QnConnectionInfo;

class QnWorkbenchConnectHandler : public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QObject base_type;

public:
    explicit QnWorkbenchConnectHandler(QObject *parent = 0);
    ~QnWorkbenchConnectHandler();

protected:
    ec2::AbstractECConnectionPtr connection2() const;
    QnLoginDialog *loginDialog() const;

    bool connected() const;

    bool connectToServer(const QUrl &appServerUrl, bool silent = false);
    bool disconnectFromServer(bool force);

    void hideMessageBox();
    void showLoginDialog();

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
    QPointer<QnLoginDialog> m_loginDialog;
    int m_connectingHandle;

    /** Flag that we should handle new connection. */
    bool m_readyForConnection;
};

#endif // WORKBENCH_CONNECT_HANDLER_H
