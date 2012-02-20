#include "logindialog.h"
#include "ui_camerasettingsdialog.h"

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include "core/resource/resource.h"
#include "ui/preferences/preferencesdialog.h"
#include "ui/style/skin.h"
#include "ui/device_settings/camera_schedule_widget.h"
#include "camerasettingsdialog.h"
#include "ui/device_settings/camera_motionmask_widget.h"
#include "ui/graphics/items/resource_widget.h"

#include "settings.h"

CameraSettingsDialog::CameraSettingsDialog(QnVirtualCameraResourcePtr camera, QWidget *parent):
    QDialog(parent),
    ui(new Ui::CameraSettingsDialog),
    m_camera(camera),
    m_connection (QnAppServerConnectionFactory::createConnection())
{
    ui->setupUi(this);
    ui->motionWidget->setCamera(m_camera);
    updateView();
}

CameraSettingsDialog::~CameraSettingsDialog()
{
}

void CameraSettingsDialog::requestFinished(int status, const QByteArray& errorString, QnResourceList resources, int handle)
{
    if (status == 0) {
        QDialog::accept();
    } else {
        QMessageBox::warning(this, "Can't save camera", "Can't save camera. Please try again later.");
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setEnabled(true);
    }
}

void CameraSettingsDialog::accept()
{
    ui->buttonBox->setEnabled(false);

    saveToModel();
    save();
}


void CameraSettingsDialog::reject()
{
    QDialog::reject();
}

void CameraSettingsDialog::saveToModel()
{
    m_camera->setName(ui->nameEdit->text());
    m_camera->setHostAddress(QHostAddress(ui->ipAddressEdit->text()));
    m_camera->setAuth(ui->loginEdit->text(), ui->passwordEdit->text());

    QnScheduleTaskList scheduleTasks;
    foreach (const QnScheduleTask::Data& scheduleTaskData, ui->cameraScheduleWidget->scheduleTasks())
        scheduleTasks.append(QnScheduleTask(scheduleTaskData));
    m_camera->setScheduleTasks(scheduleTasks);

	m_camera->setMotionMask(ui->motionWidget->motionMask());
}

void CameraSettingsDialog::updateView()
{
    ui->nameEdit->setText(m_camera->getName());
    ui->macAddressValueLabel->setText(m_camera->getMAC().toString());
    ui->ipAddressEdit->setText(m_camera->getHostAddress().toString());
    ui->loginEdit->setText(m_camera->getAuth().user());
    ui->passwordEdit->setText(m_camera->getAuth().password());

    ui->cameraScheduleWidget->setMaxFps(m_camera->getMaxFps());

    QList<QnScheduleTask::Data> scheduleTasks;
    foreach (const QnScheduleTask& scheduleTaskData, m_camera->getScheduleTasks())
        scheduleTasks.append(scheduleTaskData.getData());
    ui->cameraScheduleWidget->setScheduleTasks(scheduleTasks);
}

void CameraSettingsDialog::save()
{
    m_connection->saveAsync(m_camera, this, SLOT(requestFinished(int,QByteArray,QnResourceList,int)));
}

