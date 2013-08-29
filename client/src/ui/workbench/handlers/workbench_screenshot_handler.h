#ifndef QN_WORKBENCH_SCREENSHOT_HANDLER_H
#define QN_WORKBENCH_SCREENSHOT_HANDLER_H

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchScreenshotHandler: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchScreenshotHandler(QObject *parent = NULL);

private slots:
    void at_takeScreenshotAction_triggered();
};


#endif // QN_WORKBENCH_SCREENSHOT_HANDLER_H
