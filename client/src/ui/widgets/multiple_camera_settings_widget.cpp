#include "multiple_camera_settings_widget.h"
#include "ui_multiple_camera_settings_widget.h"

#include <QtGui/QMessageBox>

#include "core/resourcemanagment/resource_pool.h"
#include "core/resource/resource.h"
#include "ui/device_settings/camera_schedule_widget.h"
#include "ui/device_settings/camera_motion_mask_widget.h"
#include "ui/graphics/items/resource_widget.h"

QnMultipleCameraSettingsWidget::QnMultipleCameraSettingsWidget(QWidget *parent)
  : QWidget(parent),
    ui(new Ui::MultipleCameraSettingsWidget),
    m_connection(QnAppServerConnectionFactory::createConnection())
{
    ui->setupUi(this);

    updateFromResources();
}

QnMultipleCameraSettingsWidget::~QnMultipleCameraSettingsWidget()
{
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

void QnMultipleCameraSettingsWidget::submitToResources()
{
    m_login = ui->loginEdit->text().trimmed();
    m_password = ui->loginEdit->text().trimmed();

    foreach(QnVirtualCameraResourcePtr camera, m_cameras)
    {
        QString login = camera->getAuth().user();
        QString password = camera->getAuth().password();

        if (!m_login.isEmpty())
            login = m_login;

        if (!m_password.isEmpty())
            password = m_password;

        camera->setAuth(login, password);

        if (ui->cameraScheduleWidget->getScheduleEnabled() != Qt::PartiallyChecked)
            camera->setScheduleDisabled(ui->cameraScheduleWidget->getScheduleEnabled() == Qt::Unchecked);

        if (!ui->cameraScheduleWidget->isChangesDisabled() && !ui->cameraScheduleWidget->scheduleTasks().isEmpty())
        {
            QnScheduleTaskList scheduleTasks;
            foreach(const QnScheduleTask::Data& data, ui->cameraScheduleWidget->scheduleTasks())
                scheduleTasks.append(QnScheduleTask(data));
            camera->setScheduleTasks(scheduleTasks);
        }
    }
}

void QnMultipleCameraSettingsWidget::updateFromResources()
{

    // -1 - uninitialized, 0  - schedule enabled for all cameras,
    // 1 - flag is not equal for all cameras, 2 - schedule disabled for all cameras
    int scheduleEnabled = -1;

    QSet<QString> logins, passwords;
    foreach (QnVirtualCameraResourcePtr camera, m_cameras)
    {
        if (scheduleEnabled == -1) {
            scheduleEnabled = camera->isScheduleDisabled() ? Qt::Unchecked : Qt::Checked;
        } else if (scheduleEnabled != (camera->isScheduleDisabled() ? Qt::Unchecked : Qt::Checked)) {
            scheduleEnabled = Qt::PartiallyChecked;
        }

        logins.insert(camera->getAuth().user());
        passwords.insert(camera->getAuth().password());
    }

    if (logins.size() == 1)
        m_login = *logins.begin();

    if (passwords.size() == 1)
        m_password = *passwords.begin();

    ui->cameraScheduleWidget->setScheduleEnabled(static_cast<Qt::CheckState>(scheduleEnabled));




    int maxFps = 0;

    bool isScheduleEqual = true;

    QList<QnScheduleTask::Data> scheduleTasksData;

    foreach (const QnScheduleTask& scheduleTask, m_cameras.front()->getScheduleTasks())
        scheduleTasksData << scheduleTask.getData();

    foreach (QnVirtualCameraResourcePtr camera, m_cameras)
    {
        if (camera->getMaxFps() > maxFps)
            maxFps = camera->getMaxFps();

        QList<QnScheduleTask::Data> cameraScheduleTasksData;
        foreach (const QnScheduleTask& scheduleTask, camera->getScheduleTasks())
            cameraScheduleTasksData << scheduleTask.getData();

        if (cameraScheduleTasksData != scheduleTasksData) {
            isScheduleEqual = false;
            break;
        }
    }

    ui->loginEdit->setText(m_login);
    ui->passwordEdit->setText(m_password);

    ui->cameraScheduleWidget->setMaxFps(maxFps);

    /*
    if (needSetScheduleTasks)
    {
        ui->cameraScheduleWidget->setScheduleTasks(scheduleTasksData.toList());
    }
    else
        ui->cameraScheduleWidget->setScheduleTasks(QList<QnScheduleTask::Data>());
    */
    ui->cameraScheduleWidget->setScheduleTasks(m_cameras.front()->getScheduleTasks());
    ui->cameraScheduleWidget->setDoNotChange(!isScheduleEqual);
}

void QnMultipleCameraSettingsWidget::buttonClicked(QAbstractButton *button)
{
    if (ui->buttonBox->standardButton(button) == QDialogButtonBox::Apply)
    {
        saveToModel();
        save();
    }
}

void QnMultipleCameraSettingsWidget::save()
{
    m_connection->saveAsync(m_cameras, this, SLOT(requestFinished(int,QByteArray,QnResourceList,int)));
}

