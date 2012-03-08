#include "single_camera_settings_widget.h"
#include "ui_single_camera_settings_widget.h"

#include <QtGui/QMessageBox>

#include "core/resource/resource.h"
#include "core/resourcemanagment/resource_pool.h"
#include "ui/device_settings/camera_schedule_widget.h"
#include "ui/device_settings/camera_motion_mask_widget.h"
#include "ui/graphics/items/resource_widget.h"

QnSingleCameraSettingsWidget::QnSingleCameraSettingsWidget(QWidget *parent):
    QDialog(parent),
    ui(new Ui::SingleCameraSettingsWidget),
    m_connection(QnAppServerConnectionFactory::createConnection()),
    m_motionWidget(NULL),
    m_hasUnsubmittedData(false)
{
    ui->setupUi(this);
    
    connect(ui->tabWidget,              SIGNAL(currentChanged(int)),            this,   SLOT(at_tabWidget_currentChanged()));
    at_tabWidget_currentChanged();

    connect(ui->nameEdit,               SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dataChanged()));
    connect(ui->loginEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dataChanged()));
    connect(ui->passwordEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleTasksChanged()),         this,   SLOT(at_dataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleEnabledChanged()),       this,   SLOT(at_dataChanged()));

    updateFromResource();
}

QnSingleCameraSettingsWidget::~QnSingleCameraSettingsWidget()
{
}

const QnVirtualCameraResourcePtr &QnSingleCameraSettingsWidget::camera() const {
    return m_camera;
}

void QnSingleCameraSettingsWidget::setCamera(const QnVirtualCameraResourcePtr &camera) {
    if(m_camera == camera)
        return;

    m_camera = camera;
    
    if(m_motionWidget)
        m_motionWidget->setCamera(m_camera);

    updateFromResource();
}

Qn::CameraSettingsTab QnSingleCameraSettingsWidget::currentTab() const {
    /* Using field names here so that changes in UI file will lead to compilation errors. */

    QWidget *tab = ui->tabWidget->currentWidget();
    
    if(tab == ui->tabGeneral) {
        return Qn::GeneralSettingsTab;
    } else if(tab == ui->tabNetwork) {
        return Qn::NetworkSettingsTab;
    } else if(tab == ui->tabRecording) {
        return Qn::RecordingSettingsTab;
    } else if(tab == ui->tabMotion) {
        return Qn::MotionSettingsTab;
    } else {
        qnWarning("Current tab with index %1 was not recognized.", ui->tabWidget->currentIndex());
        return Qn::GeneralSettingsTab;
    }
}

void QnSingleCameraSettingsWidget::setCurrentTab(Qn::CameraSettingsTab tab) {
    /* Using field names here so that changes in UI file will lead to compilation errors. */

    switch(tab) {
    case Qn::GeneralSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->tabGeneral);
        break;
    case Qn::NetworkSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->tabNetwork);
        break;
    case Qn::RecordingSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->tabRecording);
        break;
    case Qn::MotionSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->tabMotion);
        break;
    default:
        qnWarning("Invalid camera settings tab '%1'.", static_cast<int>(tab));
        break;
    }
}

#if 0
void QnSingleCameraSettingsWidget::accept()
{
    if (m_camera->isScheduleDisabled() && ui->cameraScheduleWidget->getScheduleEnabled() == Qt::Checked
            && qnResPool->activeCameras() + 1 > qnLicensePool->getLicenses().totalCameras())
    {
        QString message = QString("Licenses limit exceeded (%1 of %2 used). Your schedule will not be saved.").arg(qnResPool->activeCameras() + 1).arg(qnLicensePool->getLicenses().totalCameras());
        QMessageBox::warning(this, "Can't enable recording", message);
        ui->cameraScheduleWidget->setScheduleEnabled(Qt::Unchecked);
        return;
    }

    ui->buttonBox->setEnabled(false);

    submitToResource();
    saveData();
}


void QnSingleCameraSettingsWidget::reject()
{
    QDialog::reject();
}
#endif

void QnSingleCameraSettingsWidget::submitToResource()
{
    if(!m_camera)
        return;

    m_camera->setName(ui->nameEdit->text());
    m_camera->setHostAddress(QHostAddress(ui->ipAddressEdit->text()));
    m_camera->setAuth(ui->loginEdit->text(), ui->passwordEdit->text());

    m_camera->setScheduleDisabled(ui->cameraScheduleWidget->getScheduleEnabled() == Qt::Unchecked);

    if (!ui->cameraScheduleWidget->isChangesDisabled())
    {
        QnScheduleTaskList scheduleTasks;
        foreach (const QnScheduleTask::Data& scheduleTaskData, ui->cameraScheduleWidget->scheduleTasks())
            scheduleTasks.append(QnScheduleTask(scheduleTaskData));
        m_camera->setScheduleTasks(scheduleTasks);
    }

    if(m_motionWidget)
	    m_camera->setMotionMaskList(m_motionWidget->motionMaskList(), QnDomainMemory);

    m_hasUnsubmittedData = false;
}

void QnSingleCameraSettingsWidget::updateFromResource()
{
    if(!m_camera) {
        ui->nameEdit->setText(QString());
        ui->macAddressEdit->setText(QString());
        ui->ipAddressEdit->setText(QString());
        ui->webPageLabel->setText(QString());
        ui->loginEdit->setText(QString());
        ui->passwordEdit->setText(QString());

        ui->cameraScheduleWidget->setMaxFps(0);
        ui->cameraScheduleWidget->setScheduleTasks(QnScheduleTaskList());
        ui->cameraScheduleWidget->setScheduleEnabled(Qt::Unchecked);
        
        /* Do not block editing by default if schedule task list is empty. */
        ui->cameraScheduleWidget->setChangesDisabled(false);
    } else {
        QString webPageAddress = QString(QLatin1String("http://%1")).arg(m_camera->getHostAddress().toString());

        ui->nameEdit->setText(m_camera->getName());
        ui->macAddressEdit->setText(m_camera->getMAC().toString());
        ui->ipAddressEdit->setText(m_camera->getHostAddress().toString());
        ui->webPageLabel->setText(tr("<a href=\"%1\">%2</a>").arg(webPageAddress).arg(webPageAddress));
        ui->loginEdit->setText(m_camera->getAuth().user());
        ui->passwordEdit->setText(m_camera->getAuth().password());

        ui->cameraScheduleWidget->setMaxFps(m_camera->getMaxFps());

        QList<QnScheduleTask::Data> scheduleTasks;
        foreach (const QnScheduleTask& scheduleTaskData, m_camera->getScheduleTasks())
            scheduleTasks.append(scheduleTaskData.getData());
        ui->cameraScheduleWidget->setScheduleTasks(scheduleTasks);

        ui->cameraScheduleWidget->setScheduleEnabled(m_camera->isScheduleDisabled() ? Qt::Unchecked : Qt::Checked);
    }

    m_hasUnsubmittedData = false;
}

#if 0
void QnSingleCameraSettingsWidget::saveData()
{
    m_connection->saveAsync(m_camera, this, SLOT(at_requestFinished(int,QByteArray,QnResourceList,int)));
}

void QnSingleCameraSettingsWidget::at_requestFinished(int status, const QByteArray &/*errorString*/, QnResourceList resources, int /*handle*/)
{
    if (status == 0) {
        QDialog::accept();
    } else {
        QMessageBox::warning(this, "Can't save camera", "Can't save camera. Please try again later.");
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setEnabled(true);
    }
}
#endif

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnSingleCameraSettingsWidget::at_tabWidget_currentChanged() 
{
    if(m_motionWidget != NULL)
        return;

    QWidget *motionWidget = ui->tabWidget->currentWidget()->findChild<QWidget *>(QLatin1String("motionWidget"));
    if(motionWidget == NULL)
        return;

    m_motionWidget = new QnCameraMotionMaskWidget(this);
    m_motionWidget->setCamera(m_camera);
    
    QVBoxLayout *layout = new QVBoxLayout(motionWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_motionWidget);

    connect(m_motionWidget, SIGNAL(motionMaskListChanged()), this, SLOT(at_dataChanged()));
}

void QnSingleCameraSettingsWidget::at_dataChanged() 
{
    m_hasUnsubmittedData = true;
}

