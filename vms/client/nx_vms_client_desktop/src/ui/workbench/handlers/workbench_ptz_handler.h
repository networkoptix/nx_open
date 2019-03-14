#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

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

    void at_ptzContinuousMoveAction_triggered();
    void at_ptzActivatePresetByIndexAction_triggered();

private:
    QVector3D applyRotation(const QVector3D& speed, qreal rotation) const;
    void showSetPositionWarning(const QnResourcePtr& resource);
};
