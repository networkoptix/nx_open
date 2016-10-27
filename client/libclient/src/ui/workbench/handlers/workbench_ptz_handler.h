#ifndef WORKBENCH_PTZ_HANDLER_H
#define WORKBENCH_PTZ_HANDLER_H

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchPtzHandler : public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QObject base_type;

public:
    explicit QnWorkbenchPtzHandler(QObject *parent = 0);
    ~QnWorkbenchPtzHandler();

private slots:
    void at_ptzActivatePresetAction_triggered();
    void at_ptzActivateTourAction_triggered();
    void at_ptzActivateObjectAction_triggered();

    void at_ptzSavePresetAction_triggered();
    void at_ptzManageAction_triggered();

    void at_debugCalibratePtzAction_triggered();
    void at_debugGetPtzPositionAction_triggered();

private:
    void showSetPositionWarning(const QnResourcePtr& resource);
};

#endif // WORKBENCH_PTZ_HANDLER_H
