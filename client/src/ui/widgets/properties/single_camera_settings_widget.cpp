#include "single_camera_settings_widget.h"
#include "ui_single_camera_settings_widget.h"

#include <QtCore/QBuffer>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QProcess>
#include <QtWidgets/QMessageBox>
#include <QtGui/QDesktopServices>

#include <camera/single_thumbnail_loader.h>

//TODO: #GDM #Common ask: what about constant MIN_SECOND_STREAM_FPS moving out of this module
#include <core/dataprovider/live_stream_provider.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/actions/action_parameters.h>
#include <ui/actions/action_manager.h>
#include <ui/common/read_only.h>

#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/warning_style.h>

#include <ui/widgets/properties/camera_schedule_widget.h>
#include <ui/widgets/properties/camera_motion_mask_widget.h>
#include <ui/widgets/properties/camera_settings_widget_p.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>

#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/scoped_value_rollback.h>
#include <utils/license_usage_helper.h>
#include <utils/aspect_ratio.h>
#include <utils/common/delayed.h>

#include "client/client_settings.h"

namespace {
    const QSize fisheyeThumbnailSize(0, 0); //unlimited size for better calibration
}


QnSingleCameraSettingsWidget::QnSingleCameraSettingsWidget(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d_ptr(new QnCameraSettingsWidgetPrivate()),
    ui(new Ui::SingleCameraSettingsWidget),
    m_cameraSupportsMotion(false),
    m_hasDbChanges(false),
    m_scheduleEnabledChanged(false),
    m_hasScheduleChanges(false),
    m_hasScheduleControlsChanges(false),
    m_hasMotionControlsChanges(false),
    m_readOnly(false),
    m_updating(false),
    m_motionWidget(NULL),
    m_inUpdateMaxFps(false)
{
    ui->setupUi(this);
    ui->licensingWidget->initializeContext(this);
    ui->cameraScheduleWidget->initializeContext(this);

    m_motionLayout = new QVBoxLayout(ui->motionWidget);
    m_motionLayout->setContentsMargins(0, 0, 0, 0);

    /* Set up context help. */
    setHelpTopic(this,                                                      Qn::CameraSettings_Help);

    setHelpTopic(ui->nameLabel,         ui->nameEdit,                       Qn::CameraSettings_General_Name_Help);
    setHelpTopic(ui->modelLabel,        ui->modelEdit,                      Qn::CameraSettings_General_Model_Help);
    setHelpTopic(ui->firmwareLabel,     ui->firmwareEdit,                   Qn::CameraSettings_General_Firmware_Help);
    //TODO: #Elric add context help for vendor field
    setHelpTopic(ui->addressGroupBox,                                       Qn::CameraSettings_General_Address_Help);
    setHelpTopic(ui->enableAudioCheckBox,                                   Qn::CameraSettings_General_Audio_Help);
    setHelpTopic(ui->authenticationGroupBox,                                Qn::CameraSettings_General_Auth_Help);
    setHelpTopic(ui->recordingTab,                                          Qn::CameraSettings_Recording_Help);
    setHelpTopic(ui->motionTab,                                             Qn::CameraSettings_Motion_Help);
    setHelpTopic(ui->advancedTab,                                           Qn::CameraSettings_Properties_Help);
    setHelpTopic(ui->ioSettingsTab,                                         Qn::CameraSettings_Properties_Help);
    setHelpTopic(ui->fisheyeTab,                                            Qn::CameraSettings_Dewarping_Help);

    connect(ui->tabWidget,              SIGNAL(currentChanged(int)),            this,   SLOT(at_tabWidget_currentChanged()));
    at_tabWidget_currentChanged();

    connect(ui->nameEdit,               SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dbDataChanged()));
    connect(ui->enableAudioCheckBox,    SIGNAL(stateChanged(int)),              this,   SLOT(at_dbDataChanged()));
    connect(ui->loginEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dbDataChanged()));
    connect(ui->passwordEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dbDataChanged()));

    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleTasksChanged()),         this,   SLOT(at_cameraScheduleWidget_scheduleTasksChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(recordingSettingsChanged()),     this,   SLOT(at_cameraScheduleWidget_recordingSettingsChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(gridParamsChanged()),            this,   SLOT(at_cameraScheduleWidget_gridParamsChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(controlsChangesApplied()),       this,   SLOT(at_cameraScheduleWidget_controlsChangesApplied()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleEnabledChanged(int)),    this,   SLOT(at_cameraScheduleWidget_scheduleEnabledChanged()));

    connect(ui->cameraScheduleWidget,   SIGNAL(gridParamsChanged()),            this,   SLOT(updateMaxFPS()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleEnabledChanged(int)),    this,   SLOT(at_dbDataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(archiveRangeChanged()),          this,   SLOT(at_dbDataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)), this, SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)));
    connect(ui->webPageLabel,           SIGNAL(linkActivated(const QString &)), this,   SLOT(at_linkActivated(const QString &)));
    connect(ui->motionWebPageLabel,     SIGNAL(linkActivated(const QString &)), this,   SLOT(at_linkActivated(const QString &)));

    connect(ui->cameraMotionButton,     SIGNAL(clicked(bool)),                  this,   SLOT(at_motionTypeChanged()));
    connect(ui->softwareMotionButton,   SIGNAL(clicked(bool)),                  this,   SLOT(at_motionTypeChanged()));
    connect(ui->sensitivitySlider,      SIGNAL(valueChanged(int)),              this,   SLOT(updateMotionWidgetSensitivity()));
    connect(ui->resetMotionRegionsButton,   &QPushButton::clicked,              this,   &QnSingleCameraSettingsWidget::at_resetMotionRegionsButton_clicked);
    connect(ui->pingButton,                 &QPushButton::clicked,              this,   [this]{menu()->trigger(Qn::PingAction, QnActionParameters(m_camera));});

    connect(ui->licensingWidget,         &QnLicensesProposeWidget::changed,      this,   &QnSingleCameraSettingsWidget::at_dbDataChanged);
    connect(ui->licensingWidget,         &QnLicensesProposeWidget::changed,      this,   [this] {
        ui->cameraScheduleWidget->setScheduleEnabled(ui->licensingWidget->state() == Qt::Checked);
    });

    connect(ui->expertSettingsWidget,   SIGNAL(dataChanged()),                  this,   SLOT(at_dbDataChanged()));

    connect(ui->fisheyeSettingsWidget,  SIGNAL(dataChanged()),                  this,   SLOT(at_fisheyeSettingsChanged()));

    connect(ui->imageControlWidget,     &QnImageControlWidget::fisheyeChanged,  this,   &QnSingleCameraSettingsWidget::at_fisheyeSettingsChanged);
    connect(ui->imageControlWidget,     &QnImageControlWidget::changed,         this,   &QnSingleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->ioPortSettingsWidget,  SIGNAL(dataChanged()),                  this,   SLOT(at_dbDataChanged()));

    updateFromResource(true);
}

QnSingleCameraSettingsWidget::~QnSingleCameraSettingsWidget() {
}

const QnVirtualCameraResourcePtr &QnSingleCameraSettingsWidget::camera() const {
    return m_camera;
}

void QnSingleCameraSettingsWidget::setCamera(const QnVirtualCameraResourcePtr &camera) {
    Q_D(QnCameraSettingsWidget);

    if(m_camera == camera)
        return;

    if(m_camera) {
        disconnect(m_camera, NULL, this, NULL);
    }

    m_camera = camera;
    QnVirtualCameraResourceList cameras;
    if (m_camera)
        cameras << m_camera;
    d->setCameras(cameras);
    ui->advancedSettingsWidget->setCamera(camera);

    if(m_camera) {
        connect(m_camera, SIGNAL(urlChanged(const QnResourcePtr &)),        this, SLOT(updateIpAddressText()));
        connect(m_camera, SIGNAL(resourceChanged(const QnResourcePtr &)),   this, SLOT(updateIpAddressText()));
        connect(m_camera, &QnResource::urlChanged,      this, &QnSingleCameraSettingsWidget::updateWebPageText); // TODO: #GDM also listen to hostAddress changes?
        connect(m_camera, &QnResource::resourceChanged, this, &QnSingleCameraSettingsWidget::updateWebPageText); // TODO: #GDM why?
        connect(m_camera, &QnResource::resourceChanged,   this, &QnSingleCameraSettingsWidget::updateMotionCapabilities);
    }

    updateFromResource(!isVisible());
    if (currentTab() == Qn::AdvancedCameraSettingsTab)
        ui->advancedSettingsWidget->reloadData();
}

Qn::CameraSettingsTab QnSingleCameraSettingsWidget::currentTab() const {
    /* Using field names here so that changes in UI file will lead to compilation errors. */

    QWidget *tab = ui->tabWidget->currentWidget();

    if(tab == ui->generalTab) {
        return Qn::GeneralSettingsTab;
    } else if(tab == ui->recordingTab) {
        return Qn::RecordingSettingsTab;
    } else if(tab == ui->ioSettingsTab) {
        return Qn::IOSettingsSettingsTab;
    } else if(tab == ui->motionTab) {
        return Qn::MotionSettingsTab;
    } else if(tab == ui->advancedTab) {
        return Qn::AdvancedCameraSettingsTab;
    } else if(tab == ui->expertTab) {
        return Qn::ExpertCameraSettingsTab;
    } else if(tab == ui->fisheyeTab) {
        return Qn::FisheyeCameraSettingsTab;
    } else {
        qnWarning("Current tab with index %1 was not recognized.", ui->tabWidget->currentIndex());
        return Qn::GeneralSettingsTab;
    }
}

void QnSingleCameraSettingsWidget::setCurrentTab(Qn::CameraSettingsTab tab) {
    /* Using field names here so that changes in UI file will lead to compilation errors. */

    switch(tab) {
    case Qn::GeneralSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->generalTab);
        break;
    case Qn::RecordingSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->recordingTab);
        break;
    case Qn::MotionSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->motionTab);
        break;
    case Qn::FisheyeCameraSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->fisheyeTab);
        break;
    case Qn::AdvancedCameraSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->advancedTab);
        break;
    case Qn::IOSettingsSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->ioSettingsTab);
        break;
    case Qn::ExpertCameraSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->expertTab);
        break;
    default:
        qnWarning("Invalid camera settings tab '%1'.", static_cast<int>(tab));
        break;
    }
}

void QnSingleCameraSettingsWidget::setScheduleEnabled(bool enabled) {
    ui->cameraScheduleWidget->setScheduleEnabled(enabled);
}

bool QnSingleCameraSettingsWidget::isScheduleEnabled() const {
    return ui->cameraScheduleWidget->isScheduleEnabled();
}

void QnSingleCameraSettingsWidget::submitToResource() {
    if(!m_camera)
        return;

    if (isReadOnly())
        return;

    if (hasDbChanges()) {
        QString name = ui->nameEdit->text().trimmed();
        if (!name.isEmpty())
            m_camera->setCameraName(name);  //TODO: #GDM warning message should be displayed on nameEdit textChanged, Ok/Apply buttons should be blocked.
        m_camera->setAudioEnabled(ui->enableAudioCheckBox->isChecked());

        //m_camera->setUrl(ui->ipAddressEdit->text());
        QAuthenticator loginEditAuth;
        loginEditAuth.setUser( ui->loginEdit->text().trimmed() );
        loginEditAuth.setPassword( ui->passwordEdit->text().trimmed() );
        if( m_camera->getAuth() != loginEditAuth )
            m_camera->setAuth( loginEditAuth );

        m_camera->setLicenseUsed(ui->licensingWidget->state() == Qt::Checked);
   //     if (m_camera->isDtsBased()) {
   //         m_camera->setScheduleDisabled(!ui->analogViewCheckBox->isChecked());
   //     } else {
            m_camera->setScheduleDisabled(!ui->cameraScheduleWidget->isScheduleEnabled());
   //     }

        int maxDays = ui->cameraScheduleWidget->maxRecordedDays();
        if (maxDays != QnCameraScheduleWidget::RecordedDaysDontChange) {
            m_camera->setMaxDays(maxDays);
            m_camera->setMinDays(ui->cameraScheduleWidget->minRecordedDays());
        }

        if (!m_camera->isDtsBased()) {
            if (m_hasScheduleChanges) {
                QnScheduleTaskList scheduleTasks;
                foreach (const QnScheduleTask::Data& scheduleTaskData, ui->cameraScheduleWidget->scheduleTasks())
                    scheduleTasks.append(QnScheduleTask(scheduleTaskData));
                m_camera->setScheduleTasks(scheduleTasks);
            }

            if (ui->cameraMotionButton->isChecked())
                m_camera->setMotionType(m_camera->getCameraBasedMotionType());
            else
                m_camera->setMotionType(Qn::MT_SoftwareGrid);

            submitMotionWidgetToResource();
        }

        ui->imageControlWidget->submitToResources(QnVirtualCameraResourceList() << m_camera);
        ui->expertSettingsWidget->submitToResources(QnVirtualCameraResourceList() << m_camera);
        ui->ioPortSettingsWidget->submitToResource(m_camera);

        QnMediaDewarpingParams dewarpingParams = m_camera->getDewarpingParams();
        ui->fisheyeSettingsWidget->submitToParams(dewarpingParams);
        dewarpingParams.enabled = ui->imageControlWidget->isFisheye(); //this step is really not needed as 'enabled' flag was set by imageControlWidget
        m_camera->setDewarpingParams(dewarpingParams);

        setHasDbChanges(false);
    }
}

void QnSingleCameraSettingsWidget::reject() {
    updateFromResource(true);
}

bool QnSingleCameraSettingsWidget::licensedParametersModified() const {
    if( !hasDbChanges() )
        return false;//nothing have been changed

    return m_scheduleEnabledChanged || m_hasScheduleChanges;
}

void QnSingleCameraSettingsWidget::updateFromResource(bool silent) {
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    QnVirtualCameraResourceList cameras;
    if (m_camera)
        cameras << m_camera;
    ui->licensingWidget->setCameras(cameras);

    if(!m_camera) {
        ui->nameEdit->clear();
        ui->modelEdit->clear();
        ui->firmwareEdit->clear();
        ui->vendorEdit->clear();
        ui->enableAudioCheckBox->setChecked(false);
        ui->macAddressEdit->clear();
        ui->loginEdit->clear();
        ui->passwordEdit->clear();

        ui->cameraScheduleWidget->setScheduleTasks(QnScheduleTaskList());
        ui->cameraScheduleWidget->setScheduleEnabled(false);
        ui->cameraScheduleWidget->setChangesDisabled(false); /* Do not block editing by default if schedule task list is empty. */
        ui->cameraScheduleWidget->setCameras(QnVirtualCameraResourceList());

        ui->softwareMotionButton->setEnabled(false);
        ui->cameraMotionButton->setText(QString());
        ui->cameraMotionButton->setChecked(false);
        ui->softwareMotionButton->setChecked(false);

        m_cameraSupportsMotion = false;
        ui->motionSettingsGroupBox->setEnabled(false);
        ui->motionAvailableLabel->setVisible(true);

        ui->imageControlWidget->updateFromResources(QnVirtualCameraResourceList());
    } else {
        bool hasVideo = m_camera->hasVideo(0);
        ui->nameEdit->setText(m_camera->getName());
        ui->modelEdit->setText(m_camera->getModel());
        ui->firmwareEdit->setText(m_camera->getFirmware());
        ui->vendorEdit->setText(m_camera->getVendor());
        ui->enableAudioCheckBox->setChecked(m_camera->isAudioEnabled());

        ui->enableAudioCheckBox->setEnabled(m_camera->isAudioSupported() && !m_camera->isAudioForced());

        ui->macAddressEdit->setText(m_camera->getMAC().toString());
        ui->loginEdit->setText(m_camera->getAuth().user());
        ui->passwordEdit->setText(m_camera->getAuth().password());

        bool dtsBased = m_camera->isDtsBased();
        ui->tabWidget->setTabEnabled(Qn::RecordingSettingsTab, !dtsBased);
        ui->tabWidget->setTabEnabled(Qn::MotionSettingsTab, !dtsBased && hasVideo);
        ui->tabWidget->setTabEnabled(Qn::AdvancedCameraSettingsTab, !dtsBased && hasVideo);
        ui->tabWidget->setTabEnabled(Qn::ExpertCameraSettingsTab, !dtsBased && hasVideo);
        ui->tabWidget->setTabEnabled(Qn::IOSettingsSettingsTab, camera()->getCameraCapabilities() & Qn::IOModuleCapability);

        ui->imageControlWidget->updateFromResources(cameras);

        if (!dtsBased) {
            ui->softwareMotionButton->setEnabled(m_camera->supportedMotionType() & Qn::MT_SoftwareGrid);
            if (m_camera->supportedMotionType() & (Qn::MT_HardwareGrid | Qn::MT_MotionWindow))
                ui->cameraMotionButton->setText(tr("Hardware (Camera built-in)"));
            else
                ui->cameraMotionButton->setText(tr("Do not record motion"));

            QnVirtualCameraResourceList cameras;
            cameras.push_back(m_camera);

            ui->cameraScheduleWidget->beginUpdate();
            ui->cameraScheduleWidget->setCameras(cameras);

            QList<QnScheduleTask::Data> scheduleTasks;
            foreach (const QnScheduleTask& scheduleTaskData, m_camera->getScheduleTasks())
                scheduleTasks.append(scheduleTaskData.getData());
            ui->cameraScheduleWidget->setScheduleTasks(scheduleTasks);

            ui->cameraScheduleWidget->setScheduleEnabled(!m_camera->isScheduleDisabled());

            int currentCameraFps = ui->cameraScheduleWidget->getGridMaxFps();
            if (currentCameraFps > 0)
                ui->cameraScheduleWidget->setFps(currentCameraFps);
            else
                ui->cameraScheduleWidget->setFps(m_camera->getMaxFps()/2);


            // TODO #Elric: wrong, need to get camera-specific web page
            ui->cameraMotionButton->setChecked(m_camera->getMotionType() != Qn::MT_SoftwareGrid);
            ui->softwareMotionButton->setChecked(m_camera->getMotionType() == Qn::MT_SoftwareGrid);

            updateMotionCapabilities();

            ui->cameraScheduleWidget->endUpdate(); //here gridParamsChanged() can be called that is connected to updateMaxFps() method

            
            ui->expertSettingsWidget->updateFromResources(cameras);

            if (!m_imageProvidersByResourceId.contains(m_camera->getId()))
                m_imageProvidersByResourceId[m_camera->getId()] = QnSingleThumbnailLoader::newInstance(m_camera, -1, -1, fisheyeThumbnailSize, QnSingleThumbnailLoader::JpgFormat, this);
            ui->fisheyeSettingsWidget->updateFromParams(m_camera->getDewarpingParams(), m_imageProvidersByResourceId[m_camera->getId()]);
        }
    }

    ui->tabWidget->setTabEnabled(Qn::FisheyeCameraSettingsTab, ui->imageControlWidget->isFisheye());
    
    updateMotionWidgetFromResource();
    updateMotionAvailability();
    updateIpAddressText();
    updateWebPageText();
    ui->advancedSettingsWidget->updateFromResource();
    ui->ioPortSettingsWidget->updateFromResource(m_camera);
    
    updateRecordingParamsAvailability();

    setHasDbChanges(false);
    m_scheduleEnabledChanged = false;
    m_hasScheduleControlsChanges = false;
    m_hasMotionControlsChanges = false;

    if (m_camera) {
        updateMaxFPS();

        /* Check if schedule was changed during load, e.g. limited by max fps. */
        if (!silent)
            executeDelayed([this] {
                showMaxFpsWarningIfNeeded();
        });
    }

    // Rollback the fisheye preview options. Makes no changes if params were not modified. --gdm
    QnResourceWidget* centralWidget = display()->widget(Qn::CentralRole);
    if (!m_camera || !centralWidget || centralWidget->resource() != m_camera)
        return;

    if (QnMediaResourceWidget* mediaWidget = dynamic_cast<QnMediaResourceWidget*>(centralWidget)) {
        mediaWidget->setDewarpingParams(mediaWidget->resource()->getDewarpingParams());
    }

}

void QnSingleCameraSettingsWidget::updateMotionWidgetFromResource() {
    if(!m_motionWidget)
        return;
    if (m_camera && m_camera->isDtsBased())
        return;

    disconnectFromMotionWidget();

    m_motionWidget->setCamera(m_camera);
    if(!m_camera) {
        m_motionWidget->setVisible(false);
    } else {
        m_motionWidget->setVisible(m_cameraSupportsMotion);
    }
    updateMotionWidgetNeedControlMaxRect();
    ui->sensitivitySlider->setValue(m_motionWidget->motionSensitivity());

    connectToMotionWidget();
}

void QnSingleCameraSettingsWidget::submitMotionWidgetToResource() {
    if(!m_motionWidget || !m_camera)
        return;

    m_camera->setMotionRegionList(m_motionWidget->motionRegionList());
}

bool QnSingleCameraSettingsWidget::isReadOnly() const {
    return m_readOnly;
}

void QnSingleCameraSettingsWidget::setReadOnly(bool readOnly) {
    if(m_readOnly == readOnly)
        return;

    using ::setReadOnly;
    setReadOnly(ui->nameEdit, readOnly);
    setReadOnly(ui->enableAudioCheckBox, readOnly);
    setReadOnly(ui->loginEdit, readOnly);
    setReadOnly(ui->passwordEdit, readOnly);
    setReadOnly(ui->cameraScheduleWidget, readOnly);
    setReadOnly(ui->resetMotionRegionsButton, readOnly);
    setReadOnly(ui->sensitivitySlider, readOnly);
    setReadOnly(ui->softwareMotionButton, readOnly);
    setReadOnly(ui->cameraMotionButton, readOnly);
    if(m_motionWidget)
        setReadOnly(m_motionWidget, readOnly);
    setReadOnly(ui->advancedSettingsWidget, readOnly);
    setReadOnly(ui->ioPortSettingsWidget, readOnly);
    m_readOnly = readOnly;
}

void QnSingleCameraSettingsWidget::setHasDbChanges(bool hasChanges) {
    if(m_hasDbChanges == hasChanges)
        return;

    m_hasDbChanges = hasChanges;
    if(!m_hasDbChanges)
    {
        m_scheduleEnabledChanged = false;
        m_hasScheduleChanges = false;
    }

    emit hasChangesChanged();
}

void QnSingleCameraSettingsWidget::disconnectFromMotionWidget() {
    assert(m_motionWidget);

    disconnect(m_motionWidget, NULL, this, NULL);
}

void QnSingleCameraSettingsWidget::connectToMotionWidget() {
    assert(m_motionWidget);

    connect(m_motionWidget, SIGNAL(motionRegionListChanged()), this, SLOT(at_dbDataChanged()), Qt::UniqueConnection);
    connect(m_motionWidget, SIGNAL(motionRegionListChanged()), this, SLOT(at_motionRegionListChanged()), Qt::UniqueConnection);
}

void QnSingleCameraSettingsWidget::showMaxFpsWarningIfNeeded() {
    if (!m_camera)
        return;

    int maxFps = -1;
    int maxDualStreamFps = -1;

    for (const QnScheduleTask& scheduleTask: m_camera->getScheduleTasks()) {
        switch (scheduleTask.getRecordingType()) {
        case Qn::RT_Never:
            continue;
        case Qn::RT_MotionAndLowQuality:
            maxDualStreamFps = qMax(maxDualStreamFps, scheduleTask.getFps());
            break;
        case Qn::RT_Always:
        case Qn::RT_MotionOnly:
            maxFps = qMax(maxFps, scheduleTask.getFps());
            break;
        default:
            break;
        }
    
    }

    bool hasChanges = false;

    int maxValidFps = std::numeric_limits<int>::max();
    int maxDualStreamingValidFps = maxFps;

    Q_D(QnCameraSettingsWidget);
    d->calculateMaxFps(&maxValidFps, &maxDualStreamingValidFps);

    if (maxValidFps < maxFps) {
        QMessageBox::warning(this, tr("FPS value is too high"),
            tr("Current fps in schedule grid is %1. Fps was dropped down to maximum camera fps %2.").arg(maxFps).arg(maxValidFps));
        hasChanges = true;
    }

    if (maxDualStreamingValidFps < maxDualStreamFps) {
        QMessageBox::warning(this, tr("FPS value is too high"),
            tr("For software motion 2 fps is reserved for secondary stream. Current fps in schedule grid is %1. Fps was dropped down to %2.")
            .arg(maxDualStreamFps).arg(maxDualStreamingValidFps));
        hasChanges = true;
    }

    if (hasChanges)
        at_cameraScheduleWidget_scheduleTasksChanged();
}

void QnSingleCameraSettingsWidget::updateMotionWidgetNeedControlMaxRect() {
    if(!m_motionWidget)
        return;
    bool hwMotion = m_camera && (m_camera->supportedMotionType() & (Qn::MT_HardwareGrid | Qn::MT_MotionWindow));
    m_motionWidget->setControlMaxRects(m_cameraSupportsMotion && hwMotion && !ui->softwareMotionButton->isChecked());
}

void QnSingleCameraSettingsWidget::updateRecordingParamsAvailability()
{
    if (!m_camera)
        return;
    
    ui->cameraScheduleWidget->setRecordingParamsAvailability(m_camera->hasVideo(0) && !m_camera->hasParam(lit("noRecordingParams")));
}

void QnSingleCameraSettingsWidget::updateMotionCapabilities() {
    m_cameraSupportsMotion = m_camera ? m_camera->hasMotion() : false;
    ui->motionSettingsGroupBox->setEnabled(m_cameraSupportsMotion);
    ui->motionAvailableLabel->setVisible(!m_cameraSupportsMotion);
}

void QnSingleCameraSettingsWidget::updateMotionAvailability() {
    if (m_camera && m_camera->isDtsBased())
        return;

    bool motionAvailable = true;
    if(ui->cameraMotionButton->isChecked())
        motionAvailable = m_camera && (m_camera->getCameraBasedMotionType() != Qn::MT_NoMotion);

    ui->cameraScheduleWidget->setMotionAvailable(motionAvailable);
}

void QnSingleCameraSettingsWidget::updateMotionWidgetSensitivity() {
    if(m_motionWidget)
        m_motionWidget->setMotionSensitivity(ui->sensitivitySlider->value());
    m_hasMotionControlsChanges = true;
}

bool QnSingleCameraSettingsWidget::isValidMotionRegion(){
    if (!m_motionWidget)
        return true;
    return m_motionWidget->isValidMotionRegion();
}

bool QnSingleCameraSettingsWidget::isValidSecondStream() {
    /* Do not check validness if there is no recording anyway. */
    if (!isScheduleEnabled())
        return true;

    if (!m_camera->hasDualStreaming())
        return true;

    QList<QnScheduleTask::Data> filteredTasks;
    bool usesSecondStream = false;
    foreach (const QnScheduleTask::Data& scheduleTaskData, ui->cameraScheduleWidget->scheduleTasks()) {
        QnScheduleTask::Data data(scheduleTaskData);
        if (data.m_recordType == Qn::RT_MotionAndLowQuality) {
            usesSecondStream = true;
            data.m_recordType = Qn::RT_Always;
        }
        filteredTasks.append(data);
    }

    /* There are no Motion+LQ tasks. */
    if (!usesSecondStream)
        return true;

    if (ui->expertSettingsWidget->isSecondStreamEnabled())
        return true;

    auto button = QMessageBox::warning(this,
        tr("Invalid schedule"),
        tr("Second stream is disabled on this camera. Motion + LQ option has no effect."\
        "Press \"Yes\" to change recording type to \"Always\" or \"No\" to re-enable second stream."),
        QMessageBox::StandardButtons(QMessageBox::Yes|QMessageBox::No | QMessageBox::Cancel),
        QMessageBox::Yes);
    switch (button) {
    case QMessageBox::Yes:
        ui->cameraScheduleWidget->setScheduleTasks(filteredTasks);
        return true;
    case QMessageBox::No:
        ui->expertSettingsWidget->setSecondStreamEnabled();
        return true;
    default:
        return false;
    }
    
}


void QnSingleCameraSettingsWidget::setExportScheduleButtonEnabled(bool enabled) {
    ui->cameraScheduleWidget->setExportScheduleButtonEnabled(enabled);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnSingleCameraSettingsWidget::hideEvent(QHideEvent *event) {
    base_type::hideEvent(event);

    if(m_motionWidget) {
        disconnectFromMotionWidget();
        m_motionWidget->setCamera(QnResourcePtr());
    }
}

void QnSingleCameraSettingsWidget::showEvent(QShowEvent *event) {
    base_type::showEvent(event);

    if(m_motionWidget) {
        updateMotionWidgetFromResource();
        connectToMotionWidget();
    }

    at_tabWidget_currentChanged(); /* Re-create motion widget if needed. */

    updateFromResource();
}

void QnSingleCameraSettingsWidget::at_motionTypeChanged() {
    at_dbDataChanged();

    updateMotionWidgetNeedControlMaxRect();
    updateMaxFPS();
    updateMotionAvailability();
}

void QnSingleCameraSettingsWidget::updateMaxFPS() {
    if (m_inUpdateMaxFps)
        return; /* Do not show message twice. */

    if(!m_camera)
        return; // TODO: #Elric investigate why we get here with null camera

    m_inUpdateMaxFps = true;

    Q_D(QnCameraSettingsWidget);

    int maxFps = std::numeric_limits<int>::max();
    int maxDualStreamingFps = maxFps;

    // motionType selected in GUI
    Qn::MotionType motionType = ui->cameraMotionButton->isChecked()
            ? m_camera->getCameraBasedMotionType()
            : Qn::MT_SoftwareGrid;

    d->calculateMaxFps(&maxFps, &maxDualStreamingFps, motionType);

    ui->cameraScheduleWidget->setMaxFps(maxFps, maxDualStreamingFps);
    m_inUpdateMaxFps = false;
}

void QnSingleCameraSettingsWidget::updateIpAddressText() {
    if(m_camera) {
        QString urlString = m_camera->getUrl();
        QUrl url = QUrl::fromUserInput(urlString);
        ui->ipAddressEdit->setText(!url.isEmpty() && url.isValid() ? url.host() : urlString);
    } else {
        ui->ipAddressEdit->clear();
    }
}

void QnSingleCameraSettingsWidget::updateWebPageText() {
    if(m_camera) {
        QString webPageAddress = QString(QLatin1String("http://%1")).arg(m_camera->getHostAddress());
        
        QUrl url = QUrl::fromUserInput(m_camera->getUrl());
        if(url.isValid()) {
            QUrlQuery query(url);
            int port = query.queryItemValue(lit("http_port")).toInt();
            if(port == 0)
                port = url.port(80);
            
            if (port != 80 && port > 0)
                webPageAddress += QLatin1Char(':') + QString::number(url.port());
        }

        ui->webPageLabel->setText(lit("<a href=\"%1\">%2</a>").arg(webPageAddress).arg(webPageAddress));

        if (!m_camera->isDtsBased()) {
            // TODO #Elric: wrong, need to get camera-specific web page
            ui->motionWebPageLabel->setText(lit("<a href=\"%1\">%2</a>").arg(webPageAddress).arg(webPageAddress));
        } else {
            ui->motionWebPageLabel->clear();
        }
    } else {
        ui->webPageLabel->clear();
        ui->motionWebPageLabel->clear();
    }
}

void QnSingleCameraSettingsWidget::at_resetMotionRegionsButton_clicked() {
    if (QMessageBox::warning(this,
        tr("Confirm motion regions reset"),
        tr("Are you sure you want to reset motion regions to the defaults?") + L'\n' + tr("This action CANNOT be undone!"),
        QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel),
        QMessageBox::Cancel) == QMessageBox::Cancel)
        return;

    if (m_motionWidget)
        m_motionWidget->clearMotion();
    m_hasMotionControlsChanges = false;
}

void QnSingleCameraSettingsWidget::at_motionRegionListChanged() {
    m_hasMotionControlsChanges = false;
}

void QnSingleCameraSettingsWidget::at_linkActivated(const QString &urlString) {
    QUrl url(urlString);
    if (!m_readOnly) {
        url.setUserName(ui->loginEdit->text());
        url.setPassword(ui->passwordEdit->text());
    }
    QDesktopServices::openUrl(url);
}

void QnSingleCameraSettingsWidget::at_tabWidget_currentChanged() {
    switch( currentTab() )
    {
        case Qn::MotionSettingsTab:
        {
            if(m_motionWidget != NULL)
                return;

            m_motionWidget = new QnCameraMotionMaskWidget(this);

            updateMotionWidgetFromResource();

            using ::setReadOnly;
            setReadOnly(m_motionWidget, m_readOnly);
            //m_motionWidget->setReadOnly(m_readOnly);

            m_motionLayout->addWidget(m_motionWidget);

            connectToMotionWidget();
            break;
        }

        case Qn::AdvancedCameraSettingsTab:
        {
            ui->advancedSettingsWidget->reloadData();
            break;
        }

        case Qn::FisheyeCameraSettingsTab:
        {
            ui->fisheyeSettingsWidget->loadPreview();
            break;
        }

        default:
            break;
    }
}

void QnSingleCameraSettingsWidget::at_dbDataChanged() {
    if (m_updating)
        return;

    ui->tabWidget->setTabEnabled(Qn::FisheyeCameraSettingsTab, ui->imageControlWidget->isFisheye());
    setHasDbChanges(true);
}

void QnSingleCameraSettingsWidget::at_cameraScheduleWidget_scheduleTasksChanged() {
    at_dbDataChanged();

    m_hasScheduleChanges = true;
    m_hasScheduleControlsChanges = false;
}

void QnSingleCameraSettingsWidget::at_cameraScheduleWidget_recordingSettingsChanged() {
    at_dbDataChanged();

    m_hasScheduleChanges = true;
}

void QnSingleCameraSettingsWidget::at_cameraScheduleWidget_gridParamsChanged() {
    m_hasScheduleControlsChanges = true;
}

void QnSingleCameraSettingsWidget::at_cameraScheduleWidget_controlsChangesApplied() {
    m_hasScheduleControlsChanges = false;
}

void QnSingleCameraSettingsWidget::at_cameraScheduleWidget_scheduleEnabledChanged() {
    ui->licensingWidget->setState(ui->cameraScheduleWidget->isScheduleEnabled() ? Qt::Checked : Qt::Unchecked);
    m_scheduleEnabledChanged = true;
}

void QnSingleCameraSettingsWidget::at_fisheyeSettingsChanged() {
    if (m_updating)
        return;

    at_dbDataChanged();

    // Preview the changes on the central widget

    QnResourceWidget* centralWidget = display()->widget(Qn::CentralRole);
    if (!m_camera || !centralWidget || centralWidget->resource() != m_camera)
        return;

    if (QnMediaResourceWidget* mediaWidget = dynamic_cast<QnMediaResourceWidget*>(centralWidget)) {
        QnMediaDewarpingParams dewarpingParams = mediaWidget->dewarpingParams();
        ui->fisheyeSettingsWidget->submitToParams(dewarpingParams);
        dewarpingParams.enabled = ui->imageControlWidget->isFisheye();
        mediaWidget->setDewarpingParams(dewarpingParams);

        QnWorkbenchItem *item = mediaWidget->item();
        QnItemDewarpingParams itemParams = item->dewarpingParams();
        itemParams.enabled = dewarpingParams.enabled;
        item->setDewarpingParams(itemParams);
    }
}
