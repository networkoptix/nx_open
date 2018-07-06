#pragma once

#include <array>

#include <client/client_color_types.h>

#include <core/resource/server_backup_schedule.h>

#include <ui/dialogs/common/session_aware_dialog.h>

class QComboBox;

namespace Ui {
    class BackupScheduleDialog;
}

class QnBackupScheduleDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    Q_PROPERTY(QnBackupScheduleColors colors READ colors WRITE setColors)

    typedef QnSessionAwareButtonBoxDialog base_type;

public:
    QnBackupScheduleDialog(QWidget *parent);
    virtual ~QnBackupScheduleDialog();

    void updateFromSettings(const QnServerBackupSchedule& value);
    void submitToSettings(QnServerBackupSchedule& value);

    const QnBackupScheduleColors &colors() const;
    void setColors(const QnBackupScheduleColors &colors);

protected:
    virtual void setReadOnlyInternal() override;

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
