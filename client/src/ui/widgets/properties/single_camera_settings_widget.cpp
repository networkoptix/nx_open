#include "single_camera_settings_widget.h"
#include "ui_single_camera_settings_widget.h"
#include <core/resource/media_server_resource.h>
#include "core/resource/resource_fwd.h"

#include <QtCore/QUrl>
#include <QtCore/QProcess>
#include <QtGui/QMessageBox>
#include <QtGui/QDesktopServices>
#include <QtGui/QSplitter>

//TODO: #GDM ask: what about constant MIN_SECOND_STREAM_FPS moving out of this module
#include <core/dataprovider/live_stream_provider.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/actions/action_parameters.h>
#include <ui/actions/action_manager.h>
#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/properties/camera_schedule_widget.h>
#include <ui/widgets/properties/camera_motion_mask_widget.h>
#include <ui/widgets/properties/camera_settings_widget_p.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/style/warning_style.h>

#include <utils/license_usage_helper.h>


QnSingleCameraSettingsWidget::QnSingleCameraSettingsWidget(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d_ptr(new QnCameraSettingsWidgetPrivate()),
    ui(new Ui::SingleCameraSettingsWidget),
    m_cameraSupportsMotion(false),
    m_hasCameraChanges(false),
    m_anyCameraChanges(false),
    m_hasDbChanges(false),
    m_hasScheduleChanges(false),
    m_hasScheduleControlsChanges(false),
    m_hasMotionControlsChanges(false),
    m_readOnly(false),
    m_motionWidget(NULL),
    m_inUpdateMaxFps(false),
    m_widgetsRecreator(0),
    m_serverConnection(0)
{
    ui->setupUi(this);

    m_motionLayout = new QVBoxLayout(ui->motionWidget);
    m_motionLayout->setContentsMargins(0, 0, 0, 0);

    ui->cameraScheduleWidget->setContext(context());

    /* Set up context help. */
    setHelpTopic(ui->nameLabel,       ui->nameEdit,                           Qn::CameraSettings_General_Name_Help);
    setHelpTopic(ui->addressGroupBox,                                         Qn::CameraSettings_General_Address_Help);
    setHelpTopic(ui->enableAudioCheckBox,                                     Qn::CameraSettings_General_Audio_Help);
    setHelpTopic(ui->authenticationGroupBox,                                  Qn::CameraSettings_General_Auth_Help);
    setHelpTopic(ui->recordingTab,                                            Qn::CameraSettings_Recording_Help);
    setHelpTopic(ui->motionTab,                                               Qn::CameraSettings_Motion_Help);
    setHelpTopic(ui->cameraPropertiesTab,                                     Qn::CameraSettings_Properties_Help);
    setHelpTopic(ui->tabAdvancedSettings,                                     Qn::CameraSettings_Expert_Help);
    setHelpTopic(ui->tabFisheyeSettings,                                      Qn::CameraSettings_Dewarping_Help);

    connect(ui->tabWidget,              SIGNAL(currentChanged(int)),            this,   SLOT(at_tabWidget_currentChanged()));
    at_tabWidget_currentChanged();

    connect(ui->nameEdit,               SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dbDataChanged()));
    connect(ui->enableAudioCheckBox,    SIGNAL(stateChanged(int)),              this,   SLOT(at_dbDataChanged()));
    connect(ui->checkBoxDewarping,      SIGNAL(stateChanged(int)),              this,   SLOT(at_dbDataChanged()));
    connect(ui->loginEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dbDataChanged()));
    connect(ui->passwordEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dbDataChanged()));

    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleTasksChanged()),         this,   SLOT(at_cameraScheduleWidget_scheduleTasksChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(recordingSettingsChanged()),     this,   SLOT(at_cameraScheduleWidget_recordingSettingsChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(gridParamsChanged()),            this,   SLOT(at_cameraScheduleWidget_gridParamsChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(controlsChangesApplied()),       this,   SLOT(at_cameraScheduleWidget_controlsChangesApplied()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleEnabledChanged(int)),    this,   SLOT(at_cameraScheduleWidget_scheduleEnabledChanged()));

    connect(ui->cameraScheduleWidget,   SIGNAL(gridParamsChanged()),            this,   SLOT(updateMaxFPS()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleEnabledChanged(int)),    this,   SLOT(at_dbDataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(moreLicensesRequested()),        this,   SIGNAL(moreLicensesRequested()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)), this, SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)));
    connect(ui->webPageLabel,           SIGNAL(linkActivated(const QString &)), this,   SLOT(at_linkActivated(const QString &)));
    connect(ui->motionWebPageLabel,     SIGNAL(linkActivated(const QString &)), this,   SLOT(at_linkActivated(const QString &)));

    connect(ui->cameraMotionButton,     SIGNAL(clicked(bool)),                  this,   SLOT(at_motionTypeChanged()));
    connect(ui->softwareMotionButton,   SIGNAL(clicked(bool)),                  this,   SLOT(at_motionTypeChanged()));
    connect(ui->sensitivitySlider,      SIGNAL(valueChanged(int)),              this,   SLOT(updateMotionWidgetSensitivity()));
    connect(ui->resetMotionRegionsButton, SIGNAL(clicked()),                    this,   SLOT(at_motionSelectionCleared()));
    connect(ui->pingButton,             SIGNAL(clicked()),                      this,   SLOT(at_pingButton_clicked()));
    connect(ui->moreLicensesButton,     SIGNAL(clicked()),                      this,   SIGNAL(moreLicensesRequested()));

    connect(ui->analogViewCheckBox,     SIGNAL(stateChanged(int)),              this,   SLOT(at_dbDataChanged()));
    connect(ui->analogViewCheckBox,     SIGNAL(stateChanged(int)),              this,   SLOT(updateLicenseText()), Qt::QueuedConnection);
    connect(qnLicensePool,              SIGNAL(licensesChanged()),              this,   SLOT(updateLicenseText()), Qt::QueuedConnection);
    connect(ui->analogViewCheckBox,     SIGNAL(clicked()),                      this,   SLOT(at_analogViewCheckBox_clicked()));

    connect(ui->advancedSettingsWidget, SIGNAL(dataChanged()),                  this,   SLOT(at_dbDataChanged()));
    connect(ui->fisheyeSettingsWidget,  SIGNAL(dataChanged()),                  this,   SLOT(at_fisheyeSettingsChanged()));

    updateFromResource();
}

QnSingleCameraSettingsWidget::~QnSingleCameraSettingsWidget() {
    cleanAdvancedSettings();
}

QnMediaServerConnectionPtr QnSingleCameraSettingsWidget::getServerConnection() const {
    if (!m_camera.isNull())
    {
        QnMediaServerResourcePtr mediaServer = qSharedPointerDynamicCast<QnMediaServerResource>(qnResPool->getResourceById(m_camera->getParentId()));
        if (mediaServer.isNull()) {
            return m_serverConnection;
        }

        m_serverConnection = QnMediaServerConnectionPtr(mediaServer->apiConnection());
    }

    return m_serverConnection;
}

void QnSingleCameraSettingsWidget::initAdvancedTab()
{
    QVariant id;
    QTreeWidget* advancedTreeWidget = 0;
    QStackedLayout* advancedLayout = 0;
    setAnyCameraChanges(false);

    if (m_camera && m_camera->getParam(QString::fromLatin1("cameraSettingsId"), id, QnDomainDatabase) && !id.isNull())
    {
        if (!m_widgetsRecreator)
        {
            QHBoxLayout *layout = dynamic_cast<QHBoxLayout*>(ui->cameraPropertiesTab->layout());
            if(!layout) {
                delete ui->cameraPropertiesTab->layout();
                ui->cameraPropertiesTab->setLayout(layout = new QHBoxLayout());
            }

            QSplitter* advancedSplitter = new QSplitter();
            advancedSplitter->setChildrenCollapsible(false);

            layout->addWidget(advancedSplitter);

            advancedTreeWidget = new QTreeWidget();
            advancedTreeWidget->setColumnCount(1);
            advancedTreeWidget->setHeaderLabel(QString::fromLatin1("Category"));

            QWidget* advancedWidget = new QWidget();
            advancedLayout = new QStackedLayout(advancedWidget);
            //Default - show empty widget, ind = 0
            advancedLayout->addWidget(new QWidget());

            advancedSplitter->addWidget(advancedTreeWidget);
            advancedSplitter->addWidget(advancedWidget);

            QList<int> sizes = advancedSplitter->sizes();
            sizes[0] = 200;
            sizes[1] = 400;
            advancedSplitter->setSizes(sizes);
        } else {
            if (m_camera->getUniqueId() == m_widgetsRecreator->getCameraId()) {
                return;
            }

            if (id == m_widgetsRecreator->getId()) {

                //ToDo: disable all and return. Currently, will do the same as for another type of cameras
                //disable;
                //return;
            }

            advancedTreeWidget = m_widgetsRecreator->getRootWidget();
            advancedLayout = m_widgetsRecreator->getRootLayout();
            cleanAdvancedSettings();
        }

        m_widgetsRecreator = new CameraSettingsWidgetsTreeCreator(m_camera->getUniqueId(), id.toString(), *advancedTreeWidget, *advancedLayout, this);
    }
    else if (m_widgetsRecreator)
    {
        advancedTreeWidget = m_widgetsRecreator->getRootWidget();
        advancedLayout = m_widgetsRecreator->getRootLayout();

        cleanAdvancedSettings();

        //Dummy creator: required for cameras, that doesn't support advanced settings
        m_widgetsRecreator = new CameraSettingsWidgetsTreeCreator(QString(), QString(), *advancedTreeWidget, *advancedLayout, this);
    }
}

void QnSingleCameraSettingsWidget::cleanAdvancedSettings()
{
    delete m_widgetsRecreator;
    m_widgetsRecreator = 0;
    m_cameraSettings.clear();
}

void QnSingleCameraSettingsWidget::loadAdvancedSettings()
{
    initAdvancedTab();

    QVariant id;
    if (m_camera && m_camera->getParam(QString::fromLatin1("cameraSettingsId"), id, QnDomainDatabase) && !id.isNull())
    {
        QnMediaServerConnectionPtr serverConnection = getServerConnection();
        if (serverConnection.isNull()) {
            return;
        }

        CameraSettingsTreeLister lister(id.toString());
        QStringList settings = lister.proceed();

#if 0
        if(m_widgetsRecreator) {
            m_cameraSettings.clear();
            foreach(const QString &setting, settings) {
                CameraSettingPtr tmp(new CameraSetting());
                tmp->deserializeFromStr(setting);
                m_cameraSettings.insert(tmp->getId(), tmp);
            }

            m_widgetsRecreator->proceed(&m_cameraSettings);
        }
#endif

        serverConnection->getParamsAsync(m_camera, settings, this, SLOT(at_advancedSettingsLoaded(int, const QnStringVariantPairList &, int)) );
    }
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
    d->setCameras(QnVirtualCameraResourceList() << camera);

    if(m_camera) {
        connect(m_camera, SIGNAL(urlChanged(const QnResourcePtr &)),        this, SLOT(updateIpAddressText()));
        connect(m_camera, SIGNAL(resourceChanged(const QnResourcePtr &)),   this, SLOT(updateIpAddressText()));
        connect(m_camera, SIGNAL(urlChanged(const QnResourcePtr &)),        this, SLOT(updateWebPageText())); // TODO: #Elric also listen to hostAddress changes?
        connect(m_camera, SIGNAL(resourceChanged(const QnResourcePtr &)),   this, SLOT(updateWebPageText()));
    }

    updateFromResource();
}

Qn::CameraSettingsTab QnSingleCameraSettingsWidget::currentTab() const {
    /* Using field names here so that changes in UI file will lead to compilation errors. */

    QWidget *tab = ui->tabWidget->currentWidget();

    if(tab == ui->generalTab) {
        return Qn::GeneralSettingsTab;
    } else if(tab == ui->recordingTab) {
        return Qn::RecordingSettingsTab;
    } else if(tab == ui->motionTab) {
        return Qn::MotionSettingsTab;
    } else if(tab == ui->cameraPropertiesTab) {
        return Qn::CameraPropertiesTab;
    } else if(tab == ui->tabAdvancedSettings) {
        return Qn::AdvancedCameraSettingsTab;
    } else if(tab == ui->tabFisheyeSettings) {
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
    case Qn::CameraPropertiesTab:
        ui->tabWidget->setCurrentWidget(ui->cameraPropertiesTab);
        break;
    case Qn::AdvancedCameraSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->tabAdvancedSettings);
        break;
    case Qn::FisheyeCameraSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->tabFisheyeSettings);
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

    if (hasDbChanges()) {
        m_camera->setName(ui->nameEdit->text());
        m_camera->setAudioEnabled(ui->enableAudioCheckBox->isChecked());
        m_camera->setUrl(ui->ipAddressEdit->text());
        m_camera->setAuth(ui->loginEdit->text(), ui->passwordEdit->text());

        if (m_camera->isAnalog()) {
            m_camera->setScheduleDisabled(!ui->analogViewCheckBox->isChecked());
        } else {
            m_camera->setScheduleDisabled(!ui->cameraScheduleWidget->isScheduleEnabled());
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

        QnSecondaryStreamQuality sQuality = ui->advancedSettingsWidget->secondaryStreamQuality();
        if (sQuality != SSQualityNotDefined)
            m_camera->setSecondaryStreamQuality(sQuality);
        Qt::CheckState cs = ui->advancedSettingsWidget->getCameraControl();
        if (cs != Qt::PartiallyChecked)
            m_camera->setCameraControlDisabled(cs == Qt::Checked);

        DewarpingParams dewarping = ui->fisheyeSettingsWidget->dewarpingParams();
        dewarping.enabled = ui->checkBoxDewarping->isChecked();
        m_camera->setDewarpingParams(dewarping);
        ui->fisheyeSettingsWidget->updateFromResource(m_camera);
        m_dewarpingParamsBackup = dewarping;

        setHasDbChanges(false);
    }

    if (hasCameraChanges()) {
        m_modifiedAdvancedParamsOutgoing.clear();
        m_modifiedAdvancedParamsOutgoing.append(m_modifiedAdvancedParams);
        m_modifiedAdvancedParams.clear();
        setHasCameraChanges(false);
    } else {
        setAnyCameraChanges(false);
    }
}

void QnSingleCameraSettingsWidget::reject()
{
    if (m_camera)
        m_camera->setDewarpingParams(m_dewarpingParamsBackup);
    updateFromResource();
}

void QnSingleCameraSettingsWidget::updateFromResource() {
    loadAdvancedSettings();

    if(!m_camera) {
        ui->nameEdit->setText(QString());
        ui->modelEdit->setText(QString());
        ui->firmwareEdit->setText(QString());
        ui->enableAudioCheckBox->setChecked(false);
        ui->checkBoxDewarping->setChecked(false);
        ui->macAddressEdit->setText(QString());
        ui->loginEdit->setText(QString());
        ui->passwordEdit->setText(QString());

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
        ui->analogGroupBox->setVisible(false);
    } else {
        ui->nameEdit->setText(m_camera->getName());
        ui->modelEdit->setText(m_camera->getModel());
        ui->firmwareEdit->setText(m_camera->getFirmware());
        ui->enableAudioCheckBox->setChecked(m_camera->isAudioEnabled());
        ui->checkBoxDewarping->setChecked(m_camera->getDewarpingParams().enabled);
        ui->enableAudioCheckBox->setEnabled(m_camera->isAudioSupported());
        //ui->checkBoxDewarping->setEnabled(m_camera->getPtzCapabilities() == 0);
        ui->checkBoxDewarping->setEnabled(m_camera->hasParam(lit("ptzCapabilities")));
        ui->macAddressEdit->setText(m_camera->getMAC().toString());
        ui->loginEdit->setText(m_camera->getAuth().user());
        ui->passwordEdit->setText(m_camera->getAuth().password());

        bool dtsBased = m_camera->isDtsBased();
        ui->tabWidget->setTabEnabled(Qn::RecordingSettingsTab, !dtsBased);
        ui->tabWidget->setTabEnabled(Qn::MotionSettingsTab, !dtsBased);
        ui->tabWidget->setTabEnabled(Qn::CameraPropertiesTab, !dtsBased);
        ui->tabWidget->setTabEnabled(Qn::AdvancedCameraSettingsTab, !dtsBased);

        ui->analogGroupBox->setVisible(m_camera->isAnalog());
        ui->analogViewCheckBox->setChecked(!m_camera->isScheduleDisabled());

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

            m_cameraSupportsMotion = m_camera->supportedMotionType() != Qn::MT_NoMotion;
            ui->motionSettingsGroupBox->setEnabled(m_cameraSupportsMotion);
            ui->motionAvailableLabel->setVisible(!m_cameraSupportsMotion);

            ui->cameraScheduleWidget->endUpdate(); //here gridParamsChanged() can be called that is connected to updateMaxFps() method

            ui->advancedSettingsWidget->updateFromResource(m_camera);
            ui->fisheyeSettingsWidget->updateFromResource(m_camera);
            m_dewarpingParamsBackup = m_camera->getDewarpingParams();
        }
    }

    ui->tabWidget->setTabEnabled(Qn::FisheyeCameraSettingsTab, ui->checkBoxDewarping->isChecked());

    updateMotionWidgetFromResource();
    updateMotionAvailability();
    updateLicenseText();
    updateIpAddressText();
    updateWebPageText();

    setHasDbChanges(false);
    setHasCameraChanges(false);
    m_hasScheduleControlsChanges = false;
    m_hasMotionControlsChanges = false;

    if (m_camera)
        updateMaxFPS();
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

    m_camera->setMotionRegionList(m_motionWidget->motionRegionList(), QnDomainMemory);
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
    setReadOnly(ui->checkBoxDewarping, readOnly);
    setReadOnly(ui->loginEdit, readOnly);
    setReadOnly(ui->passwordEdit, readOnly);
    setReadOnly(ui->cameraScheduleWidget, readOnly);
    setReadOnly(ui->resetMotionRegionsButton, readOnly);
    setReadOnly(ui->sensitivitySlider, readOnly);
    setReadOnly(ui->softwareMotionButton, readOnly);
    setReadOnly(ui->cameraMotionButton, readOnly);
    if(m_motionWidget)
        setReadOnly(m_motionWidget, readOnly);
    m_readOnly = readOnly;
}

void QnSingleCameraSettingsWidget::setHasDbChanges(bool hasChanges) {
    if(m_hasDbChanges == hasChanges)
        return;

    m_hasDbChanges = hasChanges;
    if(!m_hasDbChanges && !hasCameraChanges())
        m_hasScheduleChanges = false;

    emit hasChangesChanged();
}

void QnSingleCameraSettingsWidget::setHasCameraChanges(bool hasChanges) {
    if(m_hasCameraChanges == hasChanges)
        return;

    m_hasCameraChanges = hasChanges;
    if(!m_hasCameraChanges && !hasDbChanges())
        m_hasScheduleChanges = false;

    emit hasChangesChanged();
}

void QnSingleCameraSettingsWidget::setAnyCameraChanges(bool hasChanges) {
    if(m_anyCameraChanges == hasChanges)
        return;

    m_anyCameraChanges = hasChanges;
    if(!m_anyCameraChanges && !hasDbChanges())
        m_hasScheduleChanges = false;

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

void QnSingleCameraSettingsWidget::updateMotionWidgetNeedControlMaxRect() {
    if(!m_motionWidget)
        return;
    bool hwMotion = m_camera && (m_camera->supportedMotionType() & (Qn::MT_HardwareGrid | Qn::MT_MotionWindow));
    m_motionWidget->setNeedControlMaxRects(m_cameraSupportsMotion && hwMotion && !ui->softwareMotionButton->isChecked());
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

void QnSingleCameraSettingsWidget::updateLicenseText() {
    if (!m_camera || !m_camera->isAnalog())
        return;

    QnLicenseUsageHelper helper(QnVirtualCameraResourceList() << m_camera, ui->analogViewCheckBox->isChecked());

    //TODO: #GDM refactor duplicated code
    { // digital licenses
        QString usageText = tr("%n digital license(s) are used out of %1.", "", helper.usedDigital()).arg(helper.totalDigital());
        ui->digitalLicensesLabel->setText(usageText);
        QPalette palette = this->palette();
        if (!helper.isValid() && helper.requiredDigital() > 0)
            setWarningStyle(&palette);
        ui->digitalLicensesLabel->setPalette(palette);
    }

    { // analog licenses
        QString usageText = tr("%n analog license(s) are used out of %1.", "", helper.usedAnalog()).arg(helper.totalAnalog());
        ui->analogLicensesLabel->setText(usageText);
        QPalette palette = this->palette();
        if (!helper.isValid() && helper.requiredAnalog() > 0)
            setWarningStyle(&palette);
        ui->analogLicensesLabel->setPalette(palette);
    }
}

bool QnSingleCameraSettingsWidget::isValidMotionRegion(){
    if (!m_motionWidget)
        return true;
    return m_motionWidget->isValidMotionRegion();
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

void QnSingleCameraSettingsWidget::at_advancedSettingsLoaded(int status, const QnStringVariantPairList &params, int handle)
{
    Q_UNUSED(handle)

    if (!m_widgetsRecreator) {
        qWarning() << "QnSingleCameraSettingsWidget::at_advancedSettingsLoaded: widgets creator ptr is null, camera id: "
            << (m_camera == 0? QString::fromLatin1("unknown"): m_camera->getUniqueId());
        return;
    }
    if (status != 0) {
        qWarning() << "QnSingleCameraSettingsWidget::at_advancedSettingsLoaded: http status code is not OK: " << status
            << ". Camera id: " << (m_camera == 0? QString::fromLatin1("unknown"): m_camera->getUniqueId());
        return;
    }

    if (!m_camera || m_camera->getUniqueId() != m_widgetsRecreator->getCameraId()) {
        //If so, we received update for some other camera
        return;
    }

   // bool changesFound = false;
    QList<QPair<QString, QVariant> >::ConstIterator it = params.begin();

    for (; it != params.end(); ++it)
    {
  //      QString key = it->first;
        QString val = it->second.toString();

        if (!val.isEmpty())
        {
            CameraSettingPtr tmp(new CameraSetting());
            tmp->deserializeFromStr(val);

            CameraSettings::Iterator sIt = m_cameraSettings.find(tmp->getId());
            if (sIt == m_cameraSettings.end()) {
             //   changesFound = true;
                m_cameraSettings.insert(tmp->getId(), tmp);
                continue;
            }

            CameraSetting& savedVal = *(sIt.value());
            if (CameraSettingReader::isEnabled(savedVal)) {
                CameraSettingValue newVal = tmp->getCurrent();
                if (savedVal.getCurrent() != newVal) {
                //    changesFound = true;
                    savedVal.setCurrent(newVal);
                }
                continue;
            }

            if (CameraSettingReader::isEnabled(*tmp)) {
               // changesFound = true;
                m_cameraSettings.erase(sIt);
                m_cameraSettings.insert(tmp->getId(), tmp);
                continue;
            }
        }
    }

    //if (changesFound) {
        m_widgetsRecreator->proceed(&m_cameraSettings);
    //}
}

void QnSingleCameraSettingsWidget::at_pingButton_clicked() {
    menu()->trigger(Qn::PingAction, QnActionParameters(m_camera));
}

void QnSingleCameraSettingsWidget::at_analogViewCheckBox_clicked() {
    ui->cameraScheduleWidget->setScheduleEnabled(ui->analogViewCheckBox->isChecked());
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
/*
    int maxFps = m_camera->getMaxFps();
    if (ui->softwareMotionButton->isEnabled() &&  ui->softwareMotionButton->isChecked())
        maxFps -= MIN_SECOND_STREAM_FPS;

    int maxDualStreamingFps;
    if (m_camera->streamFpsSharingMethod() == Qn::shareFps)
        maxDualStreamingFps = m_camera->getMaxFps() - MIN_SECOND_STREAM_FPS;
    else
        maxDualStreamingFps = maxFps;*/

    ui->cameraScheduleWidget->setMaxFps(maxFps, maxDualStreamingFps);
    m_inUpdateMaxFps = false;
}

void QnSingleCameraSettingsWidget::updateIpAddressText() {
    if(m_camera) {
        ui->ipAddressEdit->setText(m_camera->getUrl());
    } else {
        ui->ipAddressEdit->clear();
    }
}

void QnSingleCameraSettingsWidget::updateWebPageText() {
    if(m_camera) {
        QString webPageAddress = QString(QLatin1String("http://%1")).arg(m_camera->getHostAddress());
        QUrl url = QUrl::fromUserInput(m_camera->getUrl());
        if (url.isValid() && url.port() != 80 && url.port() > 0)
            webPageAddress += QLatin1Char(':') + QString::number(url.port());

        ui->webPageLabel->setText(tr("<a href=\"%1\">%2</a>").arg(webPageAddress).arg(webPageAddress));

        if (!m_camera->isDtsBased()) {
            // TODO #Elric: wrong, need to get camera-specific web page
            ui->motionWebPageLabel->setText(tr("<a href=\"%1\">%2</a>").arg(webPageAddress).arg(webPageAddress));
        } else {
            ui->motionWebPageLabel->clear();
        }
    } else {
        ui->webPageLabel->clear();
        ui->motionWebPageLabel->clear();
    }
}

void QnSingleCameraSettingsWidget::at_motionSelectionCleared() {
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
    if(m_motionWidget != NULL)
        return;

    if(currentTab() != Qn::MotionSettingsTab)
        return;

    m_motionWidget = new QnCameraMotionMaskWidget(this);
    updateMotionWidgetFromResource();

    using ::setReadOnly;
    setReadOnly(m_motionWidget, m_readOnly);
    //m_motionWidget->setReadOnly(m_readOnly);

    m_motionLayout->addWidget(m_motionWidget);

    connectToMotionWidget();
}

void QnSingleCameraSettingsWidget::at_dbDataChanged() {
    ui->tabWidget->setTabEnabled(Qn::FisheyeCameraSettingsTab, ui->checkBoxDewarping->isChecked());
    setHasDbChanges(true);
}

void QnSingleCameraSettingsWidget::at_cameraDataChanged() {
    setHasCameraChanges(true);
}

void QnSingleCameraSettingsWidget::at_cameraScheduleWidget_scheduleTasksChanged() {
    at_dbDataChanged();
    at_cameraDataChanged();

    m_hasScheduleChanges = true;
    m_hasScheduleControlsChanges = false;
}

void QnSingleCameraSettingsWidget::at_cameraScheduleWidget_recordingSettingsChanged() {
    at_dbDataChanged();
    at_cameraDataChanged();

    m_hasScheduleChanges = true;
}

void QnSingleCameraSettingsWidget::at_cameraScheduleWidget_gridParamsChanged() {
    m_hasScheduleControlsChanges = true;
}

void QnSingleCameraSettingsWidget::at_cameraScheduleWidget_controlsChangesApplied() {
    m_hasScheduleControlsChanges = false;
}

void QnSingleCameraSettingsWidget::at_cameraScheduleWidget_scheduleEnabledChanged() {
    if (m_camera && m_camera->isAnalog())
        ui->analogViewCheckBox->setChecked(ui->cameraScheduleWidget->isScheduleEnabled());
}

void QnSingleCameraSettingsWidget::setAdvancedParam(const CameraSetting& val)
{
    m_modifiedAdvancedParams.push_back(QPair<QString, QVariant>(val.getId(), QVariant(val.serializeToStr())));
    setAnyCameraChanges(true);
    at_cameraDataChanged();
    emit advancedSettingChanged();

    if (m_widgetsRecreator && m_camera->getUniqueId() == m_widgetsRecreator->getCameraId())
    {
        m_widgetsRecreator->proceed(&m_cameraSettings);
        //We assume, that the user will not very quickly navigate through settings tree and
        //click on various settings, so we don't need get changes after setting 'em.
        //Navigating through settings tree causes 'refreshAdvancedSettings' method invocation
        //(which causes get request to mediaserver)
        //loadAdvancedSettings();
    }
}

void QnSingleCameraSettingsWidget::refreshAdvancedSettings()
{
    if (m_widgetsRecreator)
    {
        loadAdvancedSettings();
    }
}

void QnSingleCameraSettingsWidget::at_fisheyeSettingsChanged()
{
    at_dbDataChanged();
    m_camera->setDewarpingParams(ui->fisheyeSettingsWidget->dewarpingParams());

    emit fisheyeSettingChanged();
}
