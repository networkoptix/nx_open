#include "backup_settings_dialog.h"
#include "ui_backup_settings_dialog.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>

#include <ui/common/aligner.h>
#include <ui/common/read_only.h>
#include <ui/dialogs/backup_cameras_dialog.h>
#include <ui/dialogs/backup_schedule_dialog.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>
#include <ui/workaround/widgets_signals_workaround.h>


QnBackupSettingsDialog::QnBackupSettingsDialog(QWidget* parent) :
    base_type(parent),
    ui(new Ui::BackupSettingsDialog),
    m_backupNewCameras(false)
{
    ui->setupUi(this);

    ui->backupResourcesButton->setMode(QnSelectDevicesButton::Selected);

    ui->globalSettingsGroupBox->setTitle(tr("Global Settings\t(affect all servers in the system)"));

    ui->comboBoxBackupType->addItem(tr("By Schedule"), Qn::Backup_Schedule);
    ui->comboBoxBackupType->addItem(tr("Realtime"), Qn::Backup_RealTime);
    ui->comboBoxBackupType->addItem(tr("On Demand"), Qn::Backup_Manual);

    connect(ui->comboBoxBackupType, QnComboboxCurrentIndexChanged, this, [this]()
    {
        m_schedule.backupType = static_cast<Qn::BackupType>(ui->comboBoxBackupType->currentData().toInt());
        switch (m_schedule.backupType)
        {
            case Qn::Backup_Manual   : ui->stackedWidget->setCurrentWidget(ui->onDemandPage);   break;
            case Qn::Backup_Schedule : ui->stackedWidget->setCurrentWidget(ui->bySchedulePage); break;
            case Qn::Backup_RealTime : ui->stackedWidget->setCurrentWidget(ui->realtimePage);   break;
        }
    });

    connect(ui->pushButtonSchedule, &QPushButton::clicked, this, [this]()
    {
        auto scheduleDialog = new QnBackupScheduleDialog(this);
        scheduleDialog->setReadOnly(isReadOnly());
        scheduleDialog->updateFromSettings(m_schedule);
        if (scheduleDialog->exec() != QDialog::Accepted || isReadOnly())
            return;

        scheduleDialog->submitToSettings(m_schedule);
    });

    connect(ui->backupResourcesButton, &QPushButton::clicked, this, [this]()
    {
        //TODO: #vkutin #GDM #common In read-only mode display a different dialog
        // to view only selected cameras

        QScopedPointer<QnBackupCamerasDialog> dialog(new QnBackupCamerasDialog(this));
        dialog->setSelectedResources(ui->backupResourcesButton->selectedDevices());
        dialog->setBackupNewCameras(m_backupNewCameras);

        if (dialog->exec() != QDialog::Accepted || isReadOnly())
            return;

        ui->backupResourcesButton->setSelectedDevices(dialog->selectedResources().filtered<QnVirtualCameraResource>());
        m_backupNewCameras = dialog->backupNewCameras();
    });

    ui->qualityComboBox->addItem(tr("Low-Res Streams", "Cameras Backup"),
        QVariant::fromValue<Qn::CameraBackupQualities>(Qn::CameraBackup_LowQuality));
    ui->qualityComboBox->addItem(tr("Hi-Res Streams", "Cameras Backup"),
        QVariant::fromValue<Qn::CameraBackupQualities>(Qn::CameraBackup_HighQuality));
    ui->qualityComboBox->addItem(tr("All Streams", "Cameras Backup"),
        QVariant::fromValue<Qn::CameraBackupQualities>(Qn::CameraBackup_Both));

    setHelpTopic(this, Qn::ServerSettings_StoragesBackup_Help);

    setResizeToContentsMode(Qt::Vertical | Qt::Horizontal);

    auto aligner = new QnAligner(this);
    aligner->addWidgets({
        ui->whenToBackupLabel,
        ui->camerasLabel,
        ui->qualityLabel
    });
}

QnBackupSettingsDialog::~QnBackupSettingsDialog()
{
}

Qn::CameraBackupQualities QnBackupSettingsDialog::qualities() const
{
    return ui->qualityComboBox->currentData().value<Qn::CameraBackupQualities>();
}

void QnBackupSettingsDialog::setQualities(Qn::CameraBackupQualities qualities)
{
    ui->qualityComboBox->setCurrentIndex(
        ui->qualityComboBox->findData(QVariant::fromValue(qualities)));
}

const QnServerBackupSchedule& QnBackupSettingsDialog::schedule() const
{
    return m_schedule;
}

void QnBackupSettingsDialog::setSchedule(const QnServerBackupSchedule& schedule)
{
    m_schedule = schedule;

    ui->comboBoxBackupType->setCurrentIndex(
        ui->comboBoxBackupType->findData(m_schedule.backupType));
}

const QnVirtualCameraResourceList& QnBackupSettingsDialog::camerasToBackup() const
{
    return ui->backupResourcesButton->selectedDevices();
}

void QnBackupSettingsDialog::setCamerasToBackup(const QnVirtualCameraResourceList& cameras)
{
    ui->backupResourcesButton->setSelectedDevices(cameras);
}

bool QnBackupSettingsDialog::backupNewCameras() const
{
    return m_backupNewCameras;
}

void QnBackupSettingsDialog::setBackupNewCameras(bool value)
{
    m_backupNewCameras = value;
}

void QnBackupSettingsDialog::setReadOnlyInternal()
{
    base_type::setReadOnlyInternal();
    ::setReadOnly(ui->comboBoxBackupType, isReadOnly());
    ::setReadOnly(ui->qualityComboBox, isReadOnly());
    ::setReadOnly(ui->backupResourcesButton, isReadOnly());
}
