#include "login_dialog.h"
#include "ui_camera_settings_dialog.h"

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include "core/resource/resource.h"
#include "core/resourcemanagment/resource_pool.h"
#include "ui/preferences/preferencesdialog.h"
#include "ui/style/skin.h"
#include "ui/device_settings/camera_schedule_widget.h"
#include "camera_settings_dialog.h"
#include "ui/device_settings/camera_motionmask_widget.h"
#include "ui/graphics/items/resource_widget.h"

#include "settings.h"

CameraSettingsDialog::CameraSettingsDialog(QnVirtualCameraResourcePtr camera, QWidget *parent):
    QDialog(parent),
    ui(new Ui::CameraSettingsDialog),
    m_camera(camera),
    m_connection (QnAppServerConnectionFactory::createConnection()),
    m_motionWidget(NULL)
{
    ui->setupUi(this);
    
    // Do not block editing by default if schedule task list is empty
    ui->cameraScheduleWidget->setDoNotChange(false);

    connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(at_tabWidget_currentChanged()));
    
    at_tabWidget_currentChanged();

    ui->cameraScheduleWidget->setScheduleDisabled(m_camera->isScheduleDisabled() ? 2 : 0);

    updateView();
}

CameraSettingsDialog::~CameraSettingsDialog()
{
}

void CameraSettingsDialog::accept()
{
    if (m_camera->isScheduleDisabled() && ui->cameraScheduleWidget->getScheduleDisabled() == 0
            && qnResPool->activeCameras() + 1 > qnLicensePool->getLicenses().totalCameras())
    {
        QMessageBox::warning(this, "Can't save camera", "Licensed cameras limit exceeded. Please disable schedule.");
        return;
    }

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

    m_camera->setScheduleDisabled(ui->cameraScheduleWidget->getScheduleDisabled() == 2);

    if (!ui->cameraScheduleWidget->isDoNotChange())
    {
        QnScheduleTaskList scheduleTasks;
        foreach (const QnScheduleTask::Data& scheduleTaskData, ui->cameraScheduleWidget->scheduleTasks())
            scheduleTasks.append(QnScheduleTask(scheduleTaskData));
        m_camera->setScheduleTasks(scheduleTasks);
    }

    if(m_motionWidget)
	    m_camera->setMotionMaskList(m_motionWidget->motionMaskList(), QnDomainMemory);
}

void CameraSettingsDialog::updateView()
{
    ui->nameEdit->setText(m_camera->getName());
    ui->macAddressEdit->setText(m_camera->getMAC().toString());
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
    m_connection->saveAsync(m_camera, this, SLOT(at_requestFinished(int,QByteArray,QnResourceList,int)));
}

void CameraSettingsDialog::at_requestFinished(int status, const QByteArray &/*errorString*/, QnResourceList resources, int /*handle*/)
{
    if (status == 0) {
        QDialog::accept();
    } else {
        QMessageBox::warning(this, "Can't save camera", "Can't save camera. Please try again later.");
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setEnabled(true);
    }
}

void CameraSettingsDialog::at_tabWidget_currentChanged() 
{
    if(m_motionWidget != NULL)
        return;

    QWidget *motionWidget = ui->tabWidget->currentWidget()->findChild<QWidget *>(QLatin1String("motionWidget"));
    if(motionWidget == NULL)
        return;

    m_motionWidget = new QnCameraMotionMaskWidget(m_camera, this);

    QVBoxLayout *layout = new QVBoxLayout(motionWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_motionWidget);
}
