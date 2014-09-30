#ifndef WORKBENCH_INCOMPATIBLE_SERVERS_ACTION_HANDLER_H
#define WORKBENCH_INCOMPATIBLE_SERVERS_ACTION_HANDLER_H

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>
#include <utils/common/uuid.h>


class QnConnectToCurrentSystemTool;
class QnProgressDialog;
class QnMergeSystemsDialog;

class QnWorkbenchIncompatibleServersActionHandler : public Connective<QObject>, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    explicit QnWorkbenchIncompatibleServersActionHandler(QObject *parent = 0);
    ~QnWorkbenchIncompatibleServersActionHandler();

protected slots:
    void at_connectToCurrentSystemAction_triggered();
    void at_mergeSystemsAction_triggered();

private:
    void connectToCurrentSystem(const QSet<QnUuid> &targets);

private slots:
    void at_connectTool_finished(int errorCode);

private:
    QPointer<QnConnectToCurrentSystemTool> m_connectTool;
    QPointer<QnMergeSystemsDialog> m_mergeDialog;
};

#endif // WORKBENCH_INCOMPATIBLE_SERVERS_ACTION_HANDLER_H
