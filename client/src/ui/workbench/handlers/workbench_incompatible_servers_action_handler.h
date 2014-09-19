#ifndef WORKBENCH_INCOMPATIBLE_SERVERS_ACTION_HANDLER_H
#define WORKBENCH_INCOMPATIBLE_SERVERS_ACTION_HANDLER_H

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

class QnConnectToCurrentSystemTool;
class QnJoinSystemTool;
class QnProgressDialog;

class QnWorkbenchIncompatibleServersActionHandler : public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    explicit QnWorkbenchIncompatibleServersActionHandler(QObject *parent = 0);

protected slots:
    void at_connectToCurrentSystemAction_triggered();
    void at_joinOtherSystemAction_triggered();

private:
    QnJoinSystemTool *joinSystemTool();
    QnProgressDialog *progressDialog();

    void connectToCurrentSystem(const QSet<QUuid> &targets);

private slots:
    void at_connectTool_finished(int errorCode);
    void at_connectToCurrentSystemTool_canceled();
    void at_joinSystemTool_finished(int errorCode);

private:
    QPointer<QnConnectToCurrentSystemTool> m_connectTool;
    QnJoinSystemTool *m_joinSystemTool;
    QPointer<QnProgressDialog> m_progressDialog;
};

#endif // WORKBENCH_INCOMPATIBLE_SERVERS_ACTION_HANDLER_H
