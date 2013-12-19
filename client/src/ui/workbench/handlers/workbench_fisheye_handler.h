#ifndef QN_WORKBENCH_FISHEYE_HANDLER_H
#define QN_WORKBENCH_FISHEYE_HANDLER_H

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>
#include "ui/fisheye/fisheye_calibrator.h"
#include "core/resource/media_resource.h"

class QnWorkbenchFisheyeHandler: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnWorkbenchFisheyeHandler(QObject *parent = NULL);
    virtual ~QnWorkbenchFisheyeHandler();

private slots:
    void at_ptzCalibrateFisheyeAction_triggered();
    void at_analyseFinished();
private:
    QnFisheyeCalibrator m_calibrator;
    QnMediaResourcePtr m_resource;
};


#endif // QN_WORKBENCH_FISHEYE_HANDLER_H
