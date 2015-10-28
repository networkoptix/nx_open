#pragma once
#include <QtCore/QScopedPointer>
#include <QtWidgets/QDialog>

#include "core/resource/resource_fwd.h"
#include "api/model/storage_status_reply.h"
#include <ui/dialogs/workbench_state_dependent_dialog.h>
#include "ui_backup_schedule_dialog.h"

class QnBackupScheduleDialog: public QnWorkbenchStateDependentButtonBoxDialog {
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;

public:
    QnBackupScheduleDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnBackupScheduleDialog() {}

    void updateFromSettings(const QnServerBackupSchedule& value);
    void submitToSettings(QnServerBackupSchedule& value);
private:
    void setNearestValue(QComboBox* combobox, int time);
private slots:
    void at_bandwidthComboBoxChange(int index);
protected:
    virtual void accept() override;
private:
    QScopedPointer<Ui::BackupScheduleDialog> ui;
};
