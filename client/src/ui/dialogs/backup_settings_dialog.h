#pragma once
#include <QtCore/QScopedPointer>
#include <QtWidgets/QDialog>

#include "core/resource/resource_fwd.h"
#include <ui/dialogs/workbench_state_dependent_dialog.h>
#include "ui_backup_settings_dialog.h"
#include "ui/models/backup_settings_model.h"

class QnBackupSettingsDialog: public QnWorkbenchStateDependentButtonBoxDialog 
{
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;

public:
    QnBackupSettingsDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnBackupSettingsDialog() {}

    void updateFromSettings(const BackupSettingsDataList& value);
    void submitToSettings(BackupSettingsDataList& value);
private slots:
    void at_updateModelData();
    void at_modelDataChanged();
    void at_headerCheckStateChanged(Qt::CheckState state);
    void at_gridSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void at_gridItemPressed(const QModelIndex& index);
private:
    void setNearestValue(QComboBox* combobox, int time);
    void updateHeadersCheckState();
private:
    QScopedPointer<Ui::BackupSettingsDialog> ui;
    QnBackupSettingsModel* m_model;
    bool m_updatingModel;
    bool m_skipNextPressSignal;
    QModelIndex m_skipNextSelIndex;
    QModelIndex m_lastPressIndex;
};
