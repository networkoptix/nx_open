#include "multiple_camera_settings_widget.h"
#include "ui_multiple_camera_settings_widget.h"

#include <QtGui/QMessageBox>

#include "core/resourcemanagment/resource_pool.h"
#include "core/resource/resource.h"
#include "core/resource/camera_resource.h"
#include "ui/device_settings/camera_schedule_widget.h"
#include "ui/device_settings/camera_motion_mask_widget.h"
#include "ui/graphics/items/resource_widget.h"

QnMultipleCameraSettingsWidget::QnMultipleCameraSettingsWidget(QWidget *parent)
  : QWidget(parent),
    ui(new Ui::MultipleCameraSettingsWidget),
    m_hasChanges(false)
{
    ui->setupUi(this);

    connect(ui->loginEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dataChanged()));
    connect(ui->passwordEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleTasksChanged()),         this,   SLOT(at_dataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleEnabledChanged()),       this,   SLOT(at_dataChanged()));

    updateFromResources();
}

QnMultipleCameraSettingsWidget::~QnMultipleCameraSettingsWidget() {
    return;
}

const QnVirtualCameraResourceList &QnMultipleCameraSettingsWidget::cameras() const {
    return m_cameras;
}

void QnMultipleCameraSettingsWidget::setCameras(const QnVirtualCameraResourceList &cameras) {
    if(m_cameras == cameras)
        return;

    m_cameras = cameras;

    updateFromResources();
}

Qn::CameraSettingsTab QnMultipleCameraSettingsWidget::currentTab() const {
    /* Using field names here so that changes in UI file will lead to compilation errors. */
    
    QWidget *tab = ui->tabWidget->currentWidget();

    if(tab == ui->tabNetwork) {
        return Qn::NetworkSettingsTab;
    } else if(tab == ui->tabRecording) {
        return Qn::RecordingSettingsTab;
    } else {
        qnWarning("Current tab with index %1 was not recognized.", ui->tabWidget->currentIndex());
        return Qn::NetworkSettingsTab;
    }
}

void QnMultipleCameraSettingsWidget::setCurrentTab(Qn::CameraSettingsTab tab) {
    /* Using field names here so that changes in UI file will lead to compilation errors. */

    switch(tab) {
    case Qn::GeneralSettingsTab:
    case Qn::NetworkSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->tabNetwork);
        break;
    case Qn::RecordingSettingsTab:
    case Qn::MotionSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->tabRecording);
        break;
    default:
        qnWarning("Invalid camera settings tab '%1'.", static_cast<int>(tab));
        break;
    }
}

int QnMultipleCameraSettingsWidget::activeCameraCount() const {
    switch(ui->cameraScheduleWidget->getScheduleEnabled()) {
    case Qt::Checked:
        return m_cameras.size();
    case Qt::Unchecked:
        return 0;
    case Qt::PartiallyChecked: {
        int result;
        foreach (const QnVirtualCameraResourcePtr &camera, m_cameras)
            if (!camera->isScheduleDisabled())
                result++;
        return result;
    }
    default:
        return 0;
    }
}

void QnMultipleCameraSettingsWidget::setCamerasActive(bool active) {
    ui->cameraScheduleWidget->setScheduleEnabled(active ? Qt::Checked : Qt::Unchecked);
}

#if 0
void QnMultipleCameraSettingsWidget::requestFinished(int status, const QByteArray& errorString, QnResourceList resources, int handle)
{
    if (status == 0) {
        QDialog::accept();
    } else {
        QMessageBox::warning(this, "Can't save camera", "Can't save camera. Please try again later.");
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setEnabled(true);
    }
}

void QnMultipleCameraSettingsWidget::accept()
{
    int totalActiveCount = qnResPool->activeCameras();
    int editedCount = m_cameras.size();
    int activeCount = 0;
    foreach (QnVirtualCameraResourcePtr camera, m_cameras)
    {
        if (!camera->isScheduleDisabled())
            activeCount++;
    }

    if (totalActiveCount - activeCount + editedCount > qnLicensePool->getLicenses().totalCameras() && ui->cameraScheduleWidget->getScheduleEnabled() == Qt::Checked)
    {
        QString message = QString("Licenses limit exceeded (%1 of %2 used). Your schedule will be saved.").arg(totalActiveCount - activeCount + editedCount).arg(qnLicensePool->getLicenses().totalCameras());
        QMessageBox::warning(this, "Can't enable recording", message);
        ui->cameraScheduleWidget->setScheduleEnabled(Qt::Unchecked);
        return;
    }

    ui->buttonBox->setEnabled(false);

    saveToModel();
    save();
}


void QnMultipleCameraSettingsWidget::reject()
{
    QDialog::reject();
}
#endif

void QnMultipleCameraSettingsWidget::submitToResources() {
    QString login = ui->loginEdit->text().trimmed();
    QString password = ui->loginEdit->text().trimmed();

    foreach(QnVirtualCameraResourcePtr camera, m_cameras) {
        QString cameraLogin = camera->getAuth().user();
        if (!login.isEmpty())
            cameraLogin = login;

        QString cameraPassword = camera->getAuth().password();
        if (!password.isEmpty())
            cameraPassword = password;

        camera->setAuth(cameraLogin, cameraPassword);

        if (ui->cameraScheduleWidget->getScheduleEnabled() != Qt::PartiallyChecked)
            camera->setScheduleDisabled(ui->cameraScheduleWidget->getScheduleEnabled() == Qt::Unchecked);

        if (!ui->cameraScheduleWidget->isChangesDisabled() && !ui->cameraScheduleWidget->scheduleTasks().isEmpty()) {
            QnScheduleTaskList scheduleTasks;
            foreach(const QnScheduleTask::Data& data, ui->cameraScheduleWidget->scheduleTasks())
                scheduleTasks.append(QnScheduleTask(data));
            camera->setScheduleTasks(scheduleTasks);
        }
    }

    setHasChanges(false);
}

void QnMultipleCameraSettingsWidget::updateFromResources() {
    if(m_cameras.empty()) {
        ui->loginEdit->setText(QString());
        ui->loginEdit->setPlaceholderText(QString());
        ui->passwordEdit->setText(QString());
        ui->passwordEdit->setPlaceholderText(QString());
        ui->cameraScheduleWidget->setScheduleEnabled(Qt::PartiallyChecked);
        ui->cameraScheduleWidget->setMaxFps(0);
        ui->cameraScheduleWidget->setScheduleTasks(QnScheduleTaskList());
        ui->cameraScheduleWidget->setChangesDisabled(true);
    } else {
        /* Aggregate camera parameters first. */

        /* -1 for uninitialized, 0  - schedule enabled for all cameras,
         * 1 - flag is not equal for all cameras, 2 - schedule disabled for all cameras */
        int scheduleEnabled = -1;
        QSet<QString> logins, passwords;
        int maxFps = 0;
    
        foreach (QnVirtualCameraResourcePtr camera, m_cameras) {
            if (scheduleEnabled == -1) {
                scheduleEnabled = camera->isScheduleDisabled() ? Qt::Unchecked : Qt::Checked;
            } else if (scheduleEnabled != (camera->isScheduleDisabled() ? Qt::Unchecked : Qt::Checked)) {
                scheduleEnabled = Qt::PartiallyChecked;
            }

            logins.insert(camera->getAuth().user());
            passwords.insert(camera->getAuth().password());

            if (camera->getMaxFps() > maxFps)
                maxFps = camera->getMaxFps();
        }

        bool isScheduleEqual = true;
        QList<QnScheduleTask::Data> scheduleTasksData;
        foreach (const QnScheduleTask& scheduleTask, m_cameras.front()->getScheduleTasks())
            scheduleTasksData << scheduleTask.getData();

        foreach (QnVirtualCameraResourcePtr camera, m_cameras) {
            QList<QnScheduleTask::Data> cameraScheduleTasksData;
            foreach (const QnScheduleTask& scheduleTask, camera->getScheduleTasks())
                cameraScheduleTasksData << scheduleTask.getData();
            if (cameraScheduleTasksData != scheduleTasksData) {
                isScheduleEqual = false;
                break;
            }
        }


        /* Write camera parameters out. */

        if (logins.size() == 1) {
            ui->loginEdit->setText(*logins.begin());
            ui->loginEdit->setPlaceholderText(QString());
        } else {
            ui->loginEdit->setText(QString());
            ui->loginEdit->setPlaceholderText(tr("<multiple values>", "LoginEdit"));
        }

        if (passwords.size() == 1) {
            ui->passwordEdit->setText(*passwords.begin());
            ui->passwordEdit->setPlaceholderText(QString());
        } else {
            ui->passwordEdit->setText(QString());
            ui->passwordEdit->setPlaceholderText(tr("<multiple values>", "PasswordEdit"));
        }

        ui->cameraScheduleWidget->setScheduleEnabled(static_cast<Qt::CheckState>(scheduleEnabled));
        ui->cameraScheduleWidget->setMaxFps(maxFps);
        ui->cameraScheduleWidget->setScheduleTasks(m_cameras.front()->getScheduleTasks());
        ui->cameraScheduleWidget->setChangesDisabled(!isScheduleEqual);
    }

    setHasChanges(false);
}

void QnMultipleCameraSettingsWidget::setHasChanges(bool hasChanges) {
    if(m_hasChanges == hasChanges)
        return;

    m_hasChanges = hasChanges;

    emit hasChangesChanged();
}

/*
void QnMultipleCameraSettingsWidget::buttonClicked(QAbstractButton *button)
{
    if (ui->buttonBox->standardButton(button) == QDialogButtonBox::Apply)
    {
        saveToModel();
        save();
    }
}
*/

/*
void QnMultipleCameraSettingsWidget::save()
{
    m_connection->saveAsync(m_cameras, this, SLOT(requestFinished(int,QByteArray,QnResourceList,int)));
}
*/


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnMultipleCameraSettingsWidget::at_dataChanged() {
    setHasChanges(true);
}

