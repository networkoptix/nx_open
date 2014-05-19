#ifndef WORKBENCH_INCOMPATIBLE_SERVERS_ACTION_HANDLER_H
#define WORKBENCH_INCOMPATIBLE_SERVERS_ACTION_HANDLER_H

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

class QnConnectToCurrentSystemTool;

class QnWorkbenchIncompatibleServersActionHandler : public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    explicit QnWorkbenchIncompatibleServersActionHandler(QObject *parent = 0);

protected slots:
    void at_connectToCurrentSystemAction_triggered();

private:
    QnConnectToCurrentSystemTool *m_connectToCurrentSystemTool;
};

#endif // WORKBENCH_INCOMPATIBLE_SERVERS_ACTION_HANDLER_H
