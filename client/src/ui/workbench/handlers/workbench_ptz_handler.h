#ifndef WORKBENCH_PTZ_HANDLER_H
#define WORKBENCH_PTZ_HANDLER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/ptz/ptz_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchPtzHandler : public QObject, protected QnWorkbenchContextAware
{
    Q_OBJECT
public:
    explicit QnWorkbenchPtzHandler(QObject *parent = 0);

    typedef QObject base_type;
signals:

private slots:
    void at_ptzSavePresetAction_triggered();
    void at_ptzGoToPresetAction_triggered();
    void at_ptzManagePresetsAction_triggered();

    void at_ptzStartTourAction_triggered();
    void at_ptzManageToursAction_triggered();

    void at_debugCalibratePtzAction_triggered();
};

#endif // WORKBENCH_PTZ_HANDLER_H
