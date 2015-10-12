#include "backup_settings_dialog.h"
#include "core/resource/media_server_user_attributes.h"
#include "ui/widgets/views/checkboxed_header_view.h"

QnBackupSettingsDialog::QnBackupSettingsDialog(QWidget *parent, Qt::WindowFlags windowFlags ):
    base_type(parent, windowFlags),
    ui(new Ui::BackupSettingsDialog()),
    m_updatingModel(false)
{
    ui->setupUi(this);

    m_model = new QnBackupSettingsModel(this);
    auto sortModel = new QSortFilterProxyModel(this);
    sortModel->setSourceModel(m_model);

    QnCheckBoxedHeaderView* headers = new QnCheckBoxedHeaderView(QnBackupSettingsModel::CheckBoxColumn, this);
    headers->setVisible(true);
    headers->setSectionsClickable(true);
    headers->setSectionResizeMode(QHeaderView::ResizeToContents);
    headers->setStretchLastSection(true);
    ui->gridCameras->setHorizontalHeader(headers);
    ui->gridCameras->setModel(sortModel);

    connect (headers,           &QnCheckBoxedHeaderView::checkStateChanged, this, &QnBackupSettingsDialog::at_headerCheckStateChanged);

    connect(ui->checkBoxLQ, &QRadioButton::clicked, this, &QnBackupSettingsDialog::at_updateModelData);
    connect(ui->checkBoxHQ,  &QRadioButton::clicked, this, &QnBackupSettingsDialog::at_updateModelData);
    connect(m_model, &QAbstractItemModel::dataChanged, this, &QnBackupSettingsDialog::at_modelDataChanged);
    connect(m_model, &QAbstractItemModel::modelReset, this, &QnBackupSettingsDialog::at_modelDataChanged);
}

void QnBackupSettingsDialog::at_headerCheckStateChanged(Qt::CheckState state)
{
    if (state == Qt::PartiallyChecked)
        return;
    auto allData = m_model->modelData();
    for (auto& value: allData)
        value.isChecked = state == Qt::Checked ? true : false;
    m_model->setModelData(allData);
}

void QnBackupSettingsDialog::at_modelDataChanged()
{
    if (m_updatingModel)
        return;

    ui->checkBoxLQ->blockSignals(true);
    ui->checkBoxHQ->blockSignals(true);
    ui->gridCameras->horizontalHeader()->blockSignals(true);

    ui->checkBoxLQ->setChecked(Qt::Unchecked);
    ui->checkBoxHQ->setChecked(Qt::Unchecked);
    
    bool hasDisabled = false;
    bool hasHQ = false;
    bool hasLQ = false;
    bool hasChecked = false;
    bool hasUnchecked = false;

    for (const auto& value: m_model->modelData()) 
    {
        if (!value.isChecked) {
            hasUnchecked = true;
            continue;
        }
        hasChecked = true;

        switch(value.backupType) 
        {
        case Qn::CameraBackup_Disabled:
            hasDisabled = true;
            break;
        case Qn::CameraBackup_LowQuality:
            hasLQ = true;
            break;
        case Qn::CameraBackup_HighQuality:
            hasHQ = true;
            break;
        case Qn::CameraBackup_Both:
            hasLQ = true;
            hasHQ = true;
            break;
        default:
            break;
        }
    }

    if (hasDisabled && hasLQ)
        ui->checkBoxLQ->setCheckState(Qt::PartiallyChecked);
    else if (hasLQ)
        ui->checkBoxLQ->setCheckState(Qt::Checked);

    if (hasDisabled && hasHQ)
        ui->checkBoxHQ->setCheckState(Qt::PartiallyChecked);
    else if (hasHQ)
        ui->checkBoxHQ->setCheckState(Qt::Checked);

    auto headers = static_cast<QnCheckBoxedHeaderView*>(ui->gridCameras->horizontalHeader());
    if (hasChecked && hasUnchecked)
        headers->setCheckState(Qt::PartiallyChecked);
    else if (hasChecked)
        headers->setCheckState(Qt::Checked);
    else
        headers->setCheckState(Qt::Unchecked);

    ui->checkBoxLQ->blockSignals(false);
    ui->checkBoxHQ->blockSignals(false);
    ui->gridCameras->horizontalHeader()->blockSignals(false);
}

void QnBackupSettingsDialog::at_updateModelData()
{
    if (m_updatingModel)
        return;
    
    m_updatingModel = true;
    QCheckBox* checkBox = static_cast<QCheckBox*> (sender());
    if (checkBox->checkState() == Qt::PartiallyChecked)
        checkBox->setCheckState(Qt::Checked);

    auto modelData = m_model->modelData();
    for (auto& value: modelData) 
    {
        if (!value.isChecked)
            continue;

        if (ui->checkBoxLQ->checkState() == Qt::Checked)
            value.backupType |= Qn::CameraBackup_LowQuality;
        else if (ui->checkBoxLQ->checkState() == Qt::Unchecked)
            value.backupType &= ~Qn::CameraBackup_LowQuality;

        if (ui->checkBoxHQ->checkState() == Qt::Checked)
            value.backupType |= Qn::CameraBackup_HighQuality;
        else if (ui->checkBoxHQ->checkState() == Qt::Unchecked)
            value.backupType &= ~Qn::CameraBackup_HighQuality;
    }
    
    m_model->setModelData(modelData);
    m_updatingModel = false;
}

void QnBackupSettingsDialog::updateFromSettings(const BackupSettingsDataList& values)
{
    m_model->setModelData(values);
}

void QnBackupSettingsDialog::submitToSettings(BackupSettingsDataList& value)
{
    value = m_model->modelData();
}
