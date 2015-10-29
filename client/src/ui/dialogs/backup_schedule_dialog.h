#pragma once

#include <array>

#include <core/resource/server_backup_schedule.h>

#include <ui/dialogs/workbench_state_dependent_dialog.h>

namespace Ui {
    class BackupScheduleDialog;
}

class QnBackupScheduleDialog: public QnWorkbenchStateDependentButtonBoxDialog {
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;

public:
    QnBackupScheduleDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnBackupScheduleDialog();

    void updateFromSettings(const QnServerBackupSchedule& value);
    void submitToSettings(QnServerBackupSchedule& value);
private:
    void setNearestValue(QComboBox* combobox, int time);
    void initDayOfWeekCheckboxes();

    QCheckBox* checkboxByDayOfWeek(Qt::DayOfWeek day);

private:
    QScopedPointer<Ui::BackupScheduleDialog> ui;
    std::array<QCheckBox*, 7> m_dowCheckboxes;
};
