#pragma once

#include <array>

#include <client/client_color_types.h>

#include <core/resource/server_backup_schedule.h>

#include <ui/dialogs/common/workbench_state_dependent_dialog.h>

namespace Ui {
    class BackupScheduleDialog;
}

class QnBackupScheduleDialog: public QnWorkbenchStateDependentButtonBoxDialog {
    Q_OBJECT

    Q_PROPERTY(QnBackupScheduleColors colors READ colors WRITE setColors)

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;
public:
    QnBackupScheduleDialog(QWidget *parent = NULL);
    virtual ~QnBackupScheduleDialog();

    void updateFromSettings(const QnServerBackupSchedule& value);
    void submitToSettings(QnServerBackupSchedule& value);

    const QnBackupScheduleColors &colors() const;
    void setColors(const QnBackupScheduleColors &colors);
private:
    void setNearestValue(QComboBox* combobox, int time);
    void initDayOfWeekCheckboxes();
    void updateDayOfWeekCheckboxes();

    QCheckBox* checkboxByDayOfWeek(Qt::DayOfWeek day);

private:
    QScopedPointer<Ui::BackupScheduleDialog> ui;

    /** List of checkboxes, indexed in direct enum order, from Monday to Sunday. */
    std::array<QCheckBox*, 7> m_dowCheckboxes;

    QnBackupScheduleColors m_colors;
};
