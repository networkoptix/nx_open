#ifndef QN_WORKBENCH_SCREENSHOT_HANDLER_H
#define QN_WORKBENCH_SCREENSHOT_HANDLER_H

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

/**
 * @brief The QnWorkbenchScreenshotHandler class            Handler for the screenshots related actions.
 */
class QnWorkbenchScreenshotHandler: public QObject, protected QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnWorkbenchScreenshotHandler(QObject *parent = NULL);

private slots:
    void at_takeScreenshotAction_triggered();
};


#endif // QN_WORKBENCH_SCREENSHOT_HANDLER_H
