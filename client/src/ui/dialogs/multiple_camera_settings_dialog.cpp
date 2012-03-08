#include "login_dialog.h"
#include "ui_multiple_camera_settings_dialog.h"

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include "core/resourcemanagment/resource_pool.h"
#include "core/resource/resource.h"
#include "ui/preferences/preferencesdialog.h"
#include "ui/style/skin.h"
#include "ui/device_settings/camera_schedule_widget.h"
#include "multiple_camera_settings_dialog.h"
#include "ui/device_settings/camera_motionmask_widget.h"
#include "ui/graphics/items/resource_widget.h"

#include "settings.h"

MultipleCameraSettingsDialog::MultipleCameraSettingsDialog(QWidget *parent, QnVirtualCameraResourceList cameras)
  : QDialog(parent),
    ui(new Ui::MultipleCameraSettingsDialog),
    m_cameras(cameras),
    m_connection (QnAppServerConnectionFactory::createConnection())
{
    ui->setupUi(this);

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

    updateView();
}

MultipleCameraSettingsDialog::~MultipleCameraSettingsDialog()
{
}

void MultipleCameraSettingsDialog::requestFinished(int status, const QByteArray& errorString, QnResourceList resources, int handle)
{
    if (status == 0) {
        QDialog::accept();
    } else {
        QMessageBox::warning(this, "Can't save camera", "Can't save camera. Please try again later.");
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setEnabled(true);
    }
}

void MultipleCameraSettingsDialog::accept()
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
        QString message = QString("Licenses limit exceeded (%1 of %2 used). Your schedule will be saved.").arg(activeCount + editedCount).arg(qnLicensePool->getLicenses().totalCameras());
        QMessageBox::warning(this, "Can't enable recording", message);
        ui->cameraScheduleWidget->setScheduleEnabled(Qt::Unchecked);
        return;
    }

    ui->buttonBox->setEnabled(false);

    saveToModel();
    save();
}


void MultipleCameraSettingsDialog::reject()
{
    QDialog::reject();
}

void MultipleCameraSettingsDialog::saveToModel()
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

        if (!ui->cameraScheduleWidget->isDoNotChange() && !ui->cameraScheduleWidget->scheduleTasks().isEmpty())
        {
            QnScheduleTaskList scheduleTasks;
            foreach(const QnScheduleTask::Data& data, ui->cameraScheduleWidget->scheduleTasks())
                scheduleTasks.append(QnScheduleTask(data));
            camera->setScheduleTasks(scheduleTasks);
        }
    }
}

void MultipleCameraSettingsDialog::updateView()
{
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

void MultipleCameraSettingsDialog::buttonClicked(QAbstractButton *button)
{
    if (ui->buttonBox->standardButton(button) == QDialogButtonBox::Apply)
    {
        saveToModel();
        save();
    }
}

void MultipleCameraSettingsDialog::save()
{
    m_connection->saveAsync(m_cameras, this, SLOT(requestFinished(int,QByteArray,QnResourceList,int)));
}

