#include "backup_settings_dialog.h"
#include "core/resource/media_server_user_attributes.h"
#include "ui/widgets/views/checkboxed_header_view.h"

QnBackupSettingsDialog::QnBackupSettingsDialog(QWidget *parent, Qt::WindowFlags windowFlags ):
    base_type(parent, windowFlags),
    ui(new Ui::BackupSettingsDialog())
{
    ui->setupUi(this);

    m_model = new QnBackupSettingsModel(this);

    QnCheckBoxedHeaderView* headers = new QnCheckBoxedHeaderView(QnBackupSettingsModel::ColumnCount, this);
    headers->setVisible(true);
    headers->setSectionsClickable(false);
    headers->setSectionResizeMode(QHeaderView::ResizeToContents);
    headers->setStretchLastSection(true);
    ui->gridCameras->setHorizontalHeader(headers);
    ui->gridCameras->setModel(m_model);

    connect(ui->radioButtonNone, &QRadioButton::clicked, this, [this]() {setBackupQuality(Qn::CameraBackup_Disabled);});
    connect(ui->radioButtonLow,  &QRadioButton::clicked, this, [this]() {setBackupQuality(Qn::CameraBackup_LowQuality);});
    connect(ui->radioButtonHigh, &QRadioButton::clicked, this, [this]() {setBackupQuality(Qn::CameraBackup_HighQuality);});
    connect(ui->radioButtonAll,  &QRadioButton::clicked, this, [this]() {setBackupQuality(Qn::CameraBackup_Both);});
}

void QnBackupSettingsDialog::updateFromSettings(const BackupSettingsDataList& values)
{
    m_model->setModelData(values);
    bool isSingleValue = true;
    Qn::CameraBackupTypes singleValue;
    bool firstStep = true;

    bool hasNone = false;
    bool hasLQ = false;
    bool hasHQ = false;
    bool hasBoth = false;

    for (const auto& value: values) 
    {
        switch(value.backupType) 
        {
            case Qn::CameraBackup_Disabled:
                hasNone = true;
                break;
            case Qn::CameraBackup_HighQuality:
                hasHQ = true;
                break;
            case Qn::CameraBackup_LowQuality:
                hasLQ = true;
                break;
            case Qn::CameraBackup_Both:
                hasBoth = true;
                break;
            default:
                break;
        }

        if (firstStep) {
            singleValue = value.backupType;
            firstStep = false;
        }
        else if (value.backupType != singleValue) {
            isSingleValue = false;
        }
    }

    ui->radioButtonNone->blockSignals(true);
    ui->radioButtonLow->blockSignals(true);
    ui->radioButtonHigh->blockSignals(true);
    ui->radioButtonAll->blockSignals(true);

    ui->radioButtonNone->setChecked(Qt::Unchecked);
    ui->radioButtonLow->setChecked(Qt::Unchecked);
    ui->radioButtonHigh->setChecked(Qt::Unchecked);
    ui->radioButtonAll->setChecked(Qt::Unchecked);
    
    if (isSingleValue) 
    {
        switch(singleValue) 
        {
            case Qn::CameraBackup_Disabled:
                ui->radioButtonNone->setChecked(Qt::Checked);
                break;
            case Qn::CameraBackup_HighQuality:
                ui->radioButtonHigh->setChecked(Qt::Checked);
                break;
            case Qn::CameraBackup_LowQuality:
                ui->radioButtonLow->setChecked(Qt::Checked);
                break;
            case Qn::CameraBackup_Both:
                ui->radioButtonAll->setChecked(Qt::Checked);
                break;
            default:
                break;
        }
    }
    /*
    else {
        if (hasNone)
            ui->radioButtonNone->setChecked(Qt::PartiallyChecked);
        if (hasLQ)
            ui->radioButtonLow->setChecked(Qt::PartiallyChecked);
        if (hasHQ)
            ui->radioButtonHigh->setChecked(Qt::PartiallyChecked);
        if (hasBoth)
            ui->radioButtonAll->setChecked(Qt::PartiallyChecked);
    }
    */

    ui->radioButtonNone->blockSignals(false);
    ui->radioButtonLow->blockSignals(false);
    ui->radioButtonHigh->blockSignals(false);
    ui->radioButtonAll->blockSignals(false);
}

void QnBackupSettingsDialog::submitToSettings(BackupSettingsDataList& value)
{
    value = m_model->modelData();
}

void QnBackupSettingsDialog::setBackupQuality(Qn::CameraBackupTypes backupType)
{
    auto modelData = m_model->modelData();
    for (auto& value: modelData) {
        if (value.isChecked)
            value.backupType = backupType;
    }
    m_model->setModelData(modelData);
}
