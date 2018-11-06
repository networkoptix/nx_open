#pragma once

#include <QtCore/QPointer>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class LegacyCameraSettingsDialog;
class CameraSettingsDialog;

} // namespace nx::vms::client::desktop

class QnServerSettingsDialog;
class QnUserSettingsDialog;

class QnWorkbenchResourcesSettingsHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT

    using base_type = QObject;
public:
    QnWorkbenchResourcesSettingsHandler(QObject* parent = nullptr);
    virtual ~QnWorkbenchResourcesSettingsHandler();

private:
    void at_legacyCameraSettingsAction_triggered();
    void at_cameraSettingsAction_triggered();
    void at_serverSettingsAction_triggered();
    void at_newUserAction_triggered();
    void at_userSettingsAction_triggered();
    void at_userRolesAction_triggered();
    void at_layoutSettingsAction_triggered();
    void at_currentLayoutSettingsAction_triggered();
    void at_updateLocalFilesAction_triggered();
    void at_copyRecordingScheduleAction_triggered();

private:
    void openLayoutSettingsDialog(const QnLayoutResourcePtr& layout);

private:
    QPointer<nx::vms::client::desktop::LegacyCameraSettingsDialog> m_legacyCameraSettingsDialog;
    QPointer<nx::vms::client::desktop::CameraSettingsDialog> m_cameraSettingsDialog;
    QPointer<QnServerSettingsDialog> m_serverSettingsDialog;
    QPointer<QnUserSettingsDialog> m_userSettingsDialog;
};
