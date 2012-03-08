#include "single_camera_settings_widget.h"
#include "ui_single_camera_settings_widget.h"

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include "core/resource/resource.h"
#include "core/resourcemanagment/resource_pool.h"
#include "ui/preferences/preferencesdialog.h"
#include "ui/style/skin.h"
#include "ui/device_settings/camera_schedule_widget.h"
#include "ui/device_settings/camera_motionmask_widget.h"
#include "ui/graphics/items/resource_widget.h"

#include "settings.h"

QnSingleCameraSettingsWidget::QnSingleCameraSettingsWidget(QWidget *parent):
    QDialog(parent),
    ui(new Ui::SingleCameraSettingsWidget),
    m_connection(QnAppServerConnectionFactory::createConnection()),
    m_motionWidget(NULL),
    m_hasUnsubmittedData(false)
{
    ui->setupUi(this);
    
    connect(ui->tabWidget,      SIGNAL(currentChanged(int)),            this,   SLOT(at_tabWidget_currentChanged()));
    at_tabWidget_currentChanged();

    connect(ui->nameEdit,       SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dataChanged()));
    connect(ui->loginEdit,      SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dataChanged()));
    connect(ui->passwordEdit,   SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dataChanged()));
    connect(ui->passwordEdit,   SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dataChanged()));

    ui->

    updateData();
}

QnSingleCameraSettingsWidget::~QnSingleCameraSettingsWidget()
{
}

const QnVirtualCameraResourcePtr &QnSingleCameraSettingsWidget::camera() const {
    return m_camera;
}

void QnSingleCameraSettingsWidget::setCamera(const QnVirtualCameraResourcePtr &camera) {
    m_camera = camera;

    updateData();
}


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

    submitData();
    saveData();
}


void QnSingleCameraSettingsWidget::reject()
{
    QDialog::reject();
}

void QnSingleCameraSettingsWidget::submitData()
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

void QnSingleCameraSettingsWidget::updateData()
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

void QnSingleCameraSettingsWidget::at_tabWidget_currentChanged() 
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

void QnSingleCameraSettingsWidget::at_dataChanged() 
{
    m_hasUnsubmittedData = true;
}

