#ifndef QN_WORKBENCH_DEBUG_HANDLER_H
#define QN_WORKBENCH_DEBUG_HANDLER_H

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchDebugHandler : public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QObject base_type;

public:
    explicit QnWorkbenchDebugHandler(QObject *parent = 0);

private slots:
    void at_debugControlPanelAction_triggered();
    void at_debugIncrementCounterAction_triggered();
    void at_debugDecrementCounterAction_triggered();
    void at_debugShowResourcePoolAction_triggered();
};

#endif // QN_WORKBENCH_DEBUG_HANDLER_H
