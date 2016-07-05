#pragma once

#include <ui/workbench/workbench_context_aware.h>

class QnCameraSettingsDialog;
class QnServerSettingsDialog;
class QnUserSettingsDialog;

class QnWorkbenchResourcesSettingsHandler: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QObject base_type;

public:
    QnWorkbenchResourcesSettingsHandler(QObject *parent = nullptr);
    virtual ~QnWorkbenchResourcesSettingsHandler();

private:
    void at_cameraSettingsAction_triggered();
    void at_serverSettingsAction_triggered();
    void at_newUserAction_triggered();
    void at_userSettingsAction_triggered();
    void at_userGroupsAction_triggered();

private:
    QPointer<QnCameraSettingsDialog> m_cameraSettingsDialog;
    QPointer<QnServerSettingsDialog> m_serverSettingsDialog;
    QPointer<QnUserSettingsDialog> m_userSettingsDialog;
};
