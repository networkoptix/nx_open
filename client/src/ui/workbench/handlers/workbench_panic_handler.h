#ifndef QN_WORKBENCH_PANIC_HANDLER_H
#define QN_WORKBENCH_PANIC_HANDLER_H

#if 0

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchPanicHandler: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchPanicHandler(QObject *parent = NULL);
    virtual ~QnWorkbenchPanicHandler();

protected slots:
    void at_togglePanicModeAction_toggled(bool checked);

private:

};

#endif

#endif // QN_WORKBENCH_PANIC_HANDLER_H
