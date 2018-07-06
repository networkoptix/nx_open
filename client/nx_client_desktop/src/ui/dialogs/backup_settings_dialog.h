#pragma once

#include <QtWidgets/QDialog>

#include <core/resource/resource_fwd.h>
#include <core/resource/server_backup_schedule.h>

#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui {
    class BackupSettingsDialog;
} // namespace Ui

class QnBackupSettingsDialog : public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    explicit QnBackupSettingsDialog(QWidget* parent);
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
    QScopedPointer<Ui::BackupSettingsDialog> ui;
    QnServerBackupSchedule m_schedule;
    QnVirtualCameraResourceList m_camerasToBackup;
    bool m_backupNewCameras;
};
