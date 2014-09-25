#ifndef WORKBENCH_INCOMPATIBLE_SERVERS_ACTION_HANDLER_H
#define WORKBENCH_INCOMPATIBLE_SERVERS_ACTION_HANDLER_H

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

class QnConnectToCurrentSystemTool;
class QnProgressDialog;
class QnMergeSystemsDialog;

class QnWorkbenchIncompatibleServersActionHandler : public Connective<QObject>, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    explicit QnWorkbenchIncompatibleServersActionHandler(QObject *parent = 0);

protected slots:
    void at_connectToCurrentSystemAction_triggered();
    void at_mergeSystemsAction_triggered();

private:
    void connectToCurrentSystem(const QSet<QUuid> &targets);

private slots:
    void at_connectTool_finished(int errorCode);

private:
    QPointer<QnConnectToCurrentSystemTool> m_connectTool;
    QPointer<QnProgressDialog> m_connectProgressDialog;
    QPointer<QnMergeSystemsDialog> m_mergeDialog;
};

#endif // WORKBENCH_INCOMPATIBLE_SERVERS_ACTION_HANDLER_H
