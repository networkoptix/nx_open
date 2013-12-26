#ifndef QN_WORKBENCH_SCREENSHOT_HANDLER_H
#define QN_WORKBENCH_SCREENSHOT_HANDLER_H

#include <QtCore/QObject>

#include <common/common_globals.h>

#include <ui/workbench/workbench_context_aware.h>

class QPainter;

/**
 * @brief The QnWorkbenchScreenshotHandler class            Handler for the screenshots related actions.
 */
class QnWorkbenchScreenshotHandler: public QObject, protected QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnWorkbenchScreenshotHandler(QObject *parent = NULL);

private:
    void drawTimeStamp(QPainter *painter, const QRect &rect, Qn::Corner position, const QString &timestamp);

private slots:
    void at_takeScreenshotAction_triggered();
};


#endif // QN_WORKBENCH_SCREENSHOT_HANDLER_H
