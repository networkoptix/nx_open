#ifndef WORKBENCH_PTZ_HANDLER_H
#define WORKBENCH_PTZ_HANDLER_H

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchPtzHandler : public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QObject base_type;

public:
    explicit QnWorkbenchPtzHandler(QObject *parent = 0);

private slots:
    void at_ptzSavePresetAction_triggered();
    void at_ptzGoToPresetAction_triggered();

    void at_ptzStartTourAction_triggered();
    void at_ptzManageAction_triggered();

    void at_debugCalibratePtzAction_triggered();
    void at_debugGetPtzPositionAction_triggered();
};

#endif // WORKBENCH_PTZ_HANDLER_H
