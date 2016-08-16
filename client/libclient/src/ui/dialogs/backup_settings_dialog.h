#pragma once

#include <QtWidgets/QDialog>

#include <core/resource/resource_fwd.h>
#include <core/resource/server_backup_schedule.h>

#include <ui/dialogs/common/workbench_state_dependent_dialog.h>

namespace Ui {
    class BackupSettingsDialog;
} // namespace Ui

class QnBackupSettingsDialog : public QnWorkbenchStateDependentButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnWorkbenchStateDependentButtonBoxDialog;

public:
    explicit QnBackupSettingsDialog(QWidget* parent = nullptr);
    virtual ~QnBackupSettingsDialog();

    Qn::CameraBackupQualities qualities() const;
    void setQualities(Qn::CameraBackupQualities);

    const QnServerBackupSchedule& schedule() const;
    void setSchedule(const QnServerBackupSchedule& schedule);

    const QnVirtualCameraResourceList& camerasToBackup() const;
    void setCamerasToBackup(const QnVirtualCameraResourceList& cameras);

    bool backupNewCameras() const;
    void setBackupNewCameras(bool value);

protected:
    virtual void setReadOnlyInternal() override;

private:
    void updateCamerasButton();

private:
    QScopedPointer<Ui::BackupSettingsDialog> ui;
    QnServerBackupSchedule m_schedule;
    QnVirtualCameraResourceList m_camerasToBackup;
    bool m_backupNewCameras;
};
