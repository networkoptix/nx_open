#ifndef WORKBENCH_CONNECT_HANDLER_H
#define WORKBENCH_CONNECT_HANDLER_H

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

class QnGraphicsMessageBox;
class QnLoginDialog;

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

    bool connectToServer(const QUrl &appServerUrl);
    bool disconnectFromServer(bool force);


    bool checkCompatibility(const QnConnectionInfo &connectionInfo, ec2::ErrorCode errCode);

    bool saveState(bool force);
    void showLoginDialog();
private:
    void at_messageProcessor_connectionOpened();
    void at_messageProcessor_connectionClosed();

    void at_connectAction_triggered();
    void at_reconnectAction_triggered();
    void at_disconnectAction_triggered();

private:
    QnGraphicsMessageBox* m_connectingMessageBox;
    QPointer<QnLoginDialog> m_loginDialog;
};

#endif // WORKBENCH_CONNECT_HANDLER_H
