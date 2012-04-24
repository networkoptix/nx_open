#include "single_camera_settings_widget.h"
#include "ui_single_camera_settings_widget.h"

#include <QtGui/QMessageBox>

#include "core/resource/resource.h"
#include "core/resource/camera_resource.h"
#include "core/resourcemanagment/resource_pool.h"
#include "ui/common/read_only.h"
#include "ui/device_settings/camera_schedule_widget.h"
#include "ui/device_settings/camera_motion_mask_widget.h"
#include "ui/graphics/items/resource_widget.h"

QnSingleCameraSettingsWidget::QnSingleCameraSettingsWidget(QWidget *parent):
    QWidget(parent),
    ui(new Ui::SingleCameraSettingsWidget),
    m_motionWidget(NULL),
    m_hasChanges(false),
    m_hasScheduleChanges(false),
    m_readOnly(false)
{
    ui->setupUi(this);
    
    connect(ui->tabWidget,              SIGNAL(currentChanged(int)),            this,   SLOT(at_tabWidget_currentChanged()));
    at_tabWidget_currentChanged();

    connect(ui->nameEdit,               SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dataChanged()));
    connect(ui->loginEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dataChanged()));
    connect(ui->passwordEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleTasksChanged()),         this,   SLOT(at_cameraScheduleWidget_scheduleTasksChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleEnabledChanged()),       this,   SLOT(at_dataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(moreLicensesRequested()),        this,   SIGNAL(moreLicensesRequested()));
    connect(ui->webPageLabel,           SIGNAL(linkActivated(const QString &)), this,   SLOT(at_linkActivated(const QString &)));
    connect(ui->motionWEBPageLink,      SIGNAL(linkActivated(const QString &)), this,   SLOT(at_linkActivated(const QString &)));

    connect(ui->cameraMotionButton, SIGNAL(clicked(bool)), SLOT(at_motionTypeChanged()));
    connect(ui->softwareMotionButton, SIGNAL(clicked(bool)), SLOT(at_motionTypeChanged()));
    connect(ui->comboBoxSensetivity, SIGNAL(currentIndexChanged(int)), this, SLOT(at_motionSensitivityChanged(int)));
    connect(ui->clearAllButton, SIGNAL(clicked()), this, SLOT(at_motionSelectionCleared()));

    updateFromResource();
}

QnSingleCameraSettingsWidget::~QnSingleCameraSettingsWidget() {
    return;
}

void QnSingleCameraSettingsWidget::at_motionSensitivityChanged(int value)
{
    m_motionWidget->setMotionSensitivity(value);
}

void QnSingleCameraSettingsWidget::at_motionTypeChanged()
{
    at_dataChanged();
    updateMaxMotionRect();
}

void QnSingleCameraSettingsWidget::at_motionSelectionCleared()
{
    if (m_motionWidget)
        m_motionWidget->clearMotion();
}


void QnSingleCameraSettingsWidget::at_linkActivated(const QString &urlString)
{
    QUrl url(urlString);

    if (!m_readOnly) {
        url.setUserName(ui->loginEdit->text());
        url.setPassword(ui->passwordEdit->text());
    }

    QDesktopServices::openUrl(url);
}

const QnVirtualCameraResourcePtr &QnSingleCameraSettingsWidget::camera() const {
    return m_camera;
}

void QnSingleCameraSettingsWidget::setCamera(const QnVirtualCameraResourcePtr &camera) {
    // TODO: re-introduce camera equality check (if (camera == m_camera) return; ), 
    // move out motionwidget-related code.

    m_camera = camera;
    
    if (m_camera)
    {
        ui->softwareMotionButton->setEnabled(camera->supportedMotionType() & MT_SoftwareGrid);

        QVariant val;
        QString webPageAddress = QString("http://%1/%2").arg(m_camera->getHostAddress().toString()).arg(val.toString());
        ui->motionWEBPageLink->setText(tr("%1").arg(webPageAddress));
        if(m_motionWidget) 
            m_motionWidget->setCamera(m_camera);
        ui->softwareMotionButton->setChecked(m_camera->getMotionType() == MT_SoftwareGrid);
        updateMaxMotionRect();

        /*
        if (m_camera->getParam("motionEditURL", val, QnDomainMemory))
        {
            // motion editing is not supported. Place only reference to WEB page
            ui->motionWEBPageLink->setText(tr("<a href=\"%1\">%2</a>").arg(webPageAddress).arg(webPageAddress));
            if(m_motionWidget) {
                m_motionWidget->hide();
                ui->motionWEBPageLink->show();
                ui->motionWebPageLabel->show();
                m_motionWidget->setCamera(QnResourcePtr());
            }

        }
        else 
        {
            ui->motionWEBPageLink->setText(tr("%1").arg(webPageAddress));
            if(m_motionWidget) {
                m_motionWidget->setCamera(m_camera);
                m_motionWidget->show();
                //ui->motionWEBPageLink->hide();
                //ui->motionWebPageLabel->hide();
            }
        }
        */
    }
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

bool QnSingleCameraSettingsWidget::isCameraActive() const {
    return ui->cameraScheduleWidget->activeCameraCount() != 0;
}

void QnSingleCameraSettingsWidget::setCameraActive(bool active) {
    ui->cameraScheduleWidget->setScheduleEnabled(active);
}

void QnSingleCameraSettingsWidget::submitToResource() {
    if(!m_camera)
        return;

    m_camera->setName(ui->nameEdit->text());
    m_camera->setUrl(ui->ipAddressEdit->text());
    m_camera->setAuth(ui->loginEdit->text(), ui->passwordEdit->text());

    m_camera->setScheduleDisabled(ui->cameraScheduleWidget->activeCameraCount() == 0);

    if (m_hasScheduleChanges) {
        QnScheduleTaskList scheduleTasks;
        foreach (const QnScheduleTask::Data& scheduleTaskData, ui->cameraScheduleWidget->scheduleTasks())
            scheduleTasks.append(QnScheduleTask(scheduleTaskData));
        m_camera->setScheduleTasks(scheduleTasks);
    }

    if(m_motionWidget)
	    m_camera->setMotionRegionList(m_motionWidget->motionRegionList(), QnDomainMemory);
    if (ui->cameraMotionButton->isChecked())
        m_camera->setMotionType(m_camera->getDefaultMotionType());
    else
        m_camera->setMotionType(MT_SoftwareGrid);

    setHasChanges(false);
}

void QnSingleCameraSettingsWidget::updateFromResource() {
    if(!m_camera) {
        ui->nameEdit->setText(QString());
        ui->macAddressEdit->setText(QString());
        ui->ipAddressEdit->setText(QString());
        ui->webPageLabel->setText(QString());
        ui->loginEdit->setText(QString());
        ui->passwordEdit->setText(QString());

        ui->cameraScheduleWidget->setScheduleTasks(QnScheduleTaskList());
        ui->cameraScheduleWidget->setScheduleEnabled(false);
        
        /* Do not block editing by default if schedule task list is empty. */
        ui->cameraScheduleWidget->setChangesDisabled(false);

        ui->cameraScheduleWidget->setCameras(QnVirtualCameraResourceList());
    } else {
        QString webPageAddress = QString(QLatin1String("http://%1")).arg(m_camera->getHostAddress().toString());

        ui->nameEdit->setText(m_camera->getName());
        ui->macAddressEdit->setText(m_camera->getMAC().toString());
        ui->ipAddressEdit->setText(m_camera->getUrl());
        ui->webPageLabel->setText(tr("<a href=\"%1\">%2</a>").arg(webPageAddress).arg(webPageAddress));
        ui->loginEdit->setText(m_camera->getAuth().user());
        ui->passwordEdit->setText(m_camera->getAuth().password());

        ui->cameraScheduleWidget->setMaxFps(m_camera->getMaxFps());

        QList<QnScheduleTask::Data> scheduleTasks;
        foreach (const QnScheduleTask& scheduleTaskData, m_camera->getScheduleTasks())
            scheduleTasks.append(scheduleTaskData.getData());
        ui->cameraScheduleWidget->setScheduleTasks(scheduleTasks);

        ui->cameraScheduleWidget->setScheduleEnabled(!m_camera->isScheduleDisabled());

        QnVirtualCameraResourceList cameras;
        cameras.push_back(m_camera);
        ui->cameraScheduleWidget->setCameras(cameras);
    }

    setHasChanges(false);
}

bool QnSingleCameraSettingsWidget::isReadOnly() const {
    return m_readOnly;
}

void QnSingleCameraSettingsWidget::setReadOnly(bool readOnly) {
    if(m_readOnly == readOnly)
        return;

    using ::setReadOnly;
    setReadOnly(ui->nameEdit, readOnly);
    setReadOnly(ui->loginEdit, readOnly);
    setReadOnly(ui->passwordEdit, readOnly);
    setReadOnly(ui->cameraScheduleWidget, readOnly);
    if(m_motionWidget)
        setReadOnly(m_motionWidget, readOnly);
    m_readOnly = readOnly;
}

void QnSingleCameraSettingsWidget::setHasChanges(bool hasChanges) {
    if(m_hasChanges == hasChanges)
        return;

    m_hasChanges = hasChanges;
    if(!m_hasChanges)
        m_hasScheduleChanges = false;

    emit hasChangesChanged();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnSingleCameraSettingsWidget::at_tabWidget_currentChanged() {
    if(m_motionWidget != NULL)
        return;

    QWidget *motionWidget = ui->tabWidget->currentWidget()->findChild<QWidget *>(QLatin1String("motionWidget"));
    if(motionWidget == NULL)
        return;

    m_motionWidget = new QnCameraMotionMaskWidget(this);
    setCamera(m_camera);
    
    using ::setReadOnly;
    setReadOnly(m_motionWidget, m_readOnly);
    
    QVBoxLayout *layout = new QVBoxLayout(motionWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_motionWidget);

    connect(m_motionWidget, SIGNAL(motionRegionListChanged()), this, SLOT(at_dataChanged()));
}

void QnSingleCameraSettingsWidget::at_dataChanged() {
    setHasChanges(true);
}

void QnSingleCameraSettingsWidget::at_cameraScheduleWidget_scheduleTasksChanged() {
    at_dataChanged();

    m_hasScheduleChanges = true;
}

void QnSingleCameraSettingsWidget::updateMaxMotionRect()
{
    if (m_camera && m_motionWidget) {
        if (ui->softwareMotionButton->isChecked())
            m_motionWidget->setMaxMotionRects(INT_MAX);
        else
            m_motionWidget->setMaxMotionRects(m_camera->motionWindowCnt());
    }
}
