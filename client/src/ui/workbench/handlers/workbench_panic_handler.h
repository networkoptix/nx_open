#ifndef QN_WORKBENCH_PANIC_HANDLER_H
#define QN_WORKBENCH_PANIC_HANDLER_H

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchPanicHandler: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchPanicHandler(QObject *parent = NULL);
    virtual ~QnWorkbenchPanicHandler();


//protected slots:
    //void at_

private:

};

#endif // QN_WORKBENCH_PANIC_HANDLER_H
