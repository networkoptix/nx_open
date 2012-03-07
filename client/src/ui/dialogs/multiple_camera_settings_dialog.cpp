#include "login_dialog.h"
#include "ui_multiple_camera_settings_dialog.h"

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

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

    QSet<QString> logins, passwords;
    foreach (QnVirtualCameraResourcePtr camera, m_cameras)
    {
        logins.insert(camera->getAuth().user());
        passwords.insert(camera->getAuth().password());
    }

    if (logins.size() == 1)
        m_login = *logins.begin();

    if (passwords.size() == 1)
        m_password = *passwords.begin();

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

