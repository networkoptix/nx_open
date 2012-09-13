#include "single_camera_settings_widget.h"
#include "ui_single_camera_settings_widget.h"
//#include "core/resource/video_server.h"
#include <core/resource/video_server_resource.h>
#include "core/resource/resource_fwd.h"

#include <QtCore/QUrl>
#include <QtGui/QMessageBox>
#include <QtGui/QDesktopServices>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resourcemanagment/resource_pool.h>

#include <ui/common/read_only.h>
#include <ui/widgets/properties/camera_schedule_widget.h>
#include <ui/widgets/properties/camera_motion_mask_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>

QnSingleCameraSettingsWidget::QnSingleCameraSettingsWidget(QWidget *parent):
    QWidget(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::SingleCameraSettingsWidget),
    m_motionWidget(NULL),
    m_hasCameraChanges(false),
    m_anyCameraChanges(false),
    m_hasDbChanges(false),
    m_hasScheduleChanges(false),
    m_readOnly(false),
    m_inUpdateMaxFps(false),
    m_cameraSupportsMotion(false),
    m_widgetsRecreator(0),
    m_serverConnection(0)
{
    ui->setupUi(this);

    m_motionLayout = new QVBoxLayout(ui->motionWidget);
    m_motionLayout->setContentsMargins(0, 0, 0, 0);
    
    ui->cameraScheduleWidget->setContext(context());

    connect(ui->tabWidget,              SIGNAL(currentChanged(int)),            this,   SLOT(at_tabWidget_currentChanged()));
    at_tabWidget_currentChanged();

    connect(ui->nameEdit,               SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dbDataChanged()));
    connect(ui->checkBoxEnableAudio,    SIGNAL(stateChanged(int)),              this,   SLOT(at_dbDataChanged()));
    connect(ui->loginEdit,              SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dbDataChanged()));
    connect(ui->passwordEdit,           SIGNAL(textChanged(const QString &)),   this,   SLOT(at_dbDataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleTasksChanged()),         this,   SLOT(at_cameraScheduleWidget_scheduleTasksChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(gridParamsChanged()),            this,   SLOT(updateMaxFPS()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleEnabledChanged()),       this,   SLOT(at_dbDataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(moreLicensesRequested()),        this,   SIGNAL(moreLicensesRequested()));
    connect(ui->webPageLabel,           SIGNAL(linkActivated(const QString &)), this,   SLOT(at_linkActivated(const QString &)));
    connect(ui->motionWebPageLabel,     SIGNAL(linkActivated(const QString &)), this,   SLOT(at_linkActivated(const QString &)));

    connect(ui->cameraMotionButton,     SIGNAL(clicked(bool)),                  this,   SLOT(at_motionTypeChanged()));
    connect(ui->softwareMotionButton,   SIGNAL(clicked(bool)),                  this,   SLOT(at_motionTypeChanged()));
    connect(ui->sensitivitySlider,      SIGNAL(valueChanged(int)),              this,   SLOT(updateMotionWidgetSensitivity()));
    connect(ui->resetMotionRegionsButton, SIGNAL(clicked()),                    this,   SLOT(at_motionSelectionCleared()));

    updateFromResource();
}

QnSingleCameraSettingsWidget::~QnSingleCameraSettingsWidget() {
    cleanAdvancedSettings();
}

QnVideoServerConnectionPtr QnSingleCameraSettingsWidget::getServerConnection() const {
    if (!m_camera.isNull() && m_serverConnection.isNull())
    {
        QnVideoServerResourcePtr videoServer = qSharedPointerDynamicCast<QnVideoServerResource>(qnResPool->getResourceById(m_camera->getParentId()));
        if (videoServer.isNull()) {
            return m_serverConnection;
        }

        m_serverConnection = QnVideoServerConnectionPtr(videoServer->apiConnection());
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
            QHBoxLayout *layout = dynamic_cast<QHBoxLayout*>(ui->tabAdvanced->layout());
            if(!layout) {
                delete ui->tabAdvanced->layout();
                ui->tabAdvanced->setLayout(layout = new QHBoxLayout());
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
        QnVideoServerConnectionPtr serverConnection = getServerConnection();
        if (serverConnection.isNull()) {
            return;
        }

        CameraSettingsTreeLister lister(id.toString());
        QStringList settings = lister.proceed();

        qRegisterMetaType<QList<QPair<QString, QVariant> > >("QList<QPair<QString, QVariant> >");
        serverConnection->asyncGetParamList(m_camera, settings, this, SLOT(at_advancedSettingsLoaded(int, const QList<QPair<QString, QVariant> >&)) );
    }
}

const QnVirtualCameraResourcePtr &QnSingleCameraSettingsWidget::camera() const {
    return m_camera;
}

void QnSingleCameraSettingsWidget::setCamera(const QnVirtualCameraResourcePtr &camera) {
    if(m_camera == camera)
        return;

    m_camera = camera;
    
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
    } else if(tab == ui->tabAdvanced) {
        return Qn::AdvancedSettingsTab;
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
    case Qn::AdvancedSettingsTab:
        ui->tabWidget->setCurrentWidget(ui->tabAdvanced);
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

    if (hasDbChanges()) {
        m_camera->setName(ui->nameEdit->text());
        m_camera->setAudioEnabled(ui->checkBoxEnableAudio->isChecked());
        m_camera->setUrl(ui->ipAddressEdit->text());
        m_camera->setAuth(ui->loginEdit->text(), ui->passwordEdit->text());
        m_camera->setScheduleDisabled(ui->cameraScheduleWidget->activeCameraCount() == 0);

        if (m_hasScheduleChanges) {
            QnScheduleTaskList scheduleTasks;
            foreach (const QnScheduleTask::Data& scheduleTaskData, ui->cameraScheduleWidget->scheduleTasks())
                scheduleTasks.append(QnScheduleTask(scheduleTaskData));
            m_camera->setScheduleTasks(scheduleTasks);
        }

        if (ui->cameraMotionButton->isChecked())
            m_camera->setMotionType(m_camera->getCameraBasedMotionType());
        else
            m_camera->setMotionType(MT_SoftwareGrid);

        submitMotionWidgetToResource();

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

void QnSingleCameraSettingsWidget::updateFromResource() {
    loadAdvancedSettings();

    if(!m_camera) {
        ui->nameEdit->setText(QString());
        ui->checkBoxEnableAudio->setChecked(false);
        ui->macAddressEdit->setText(QString());
        ui->ipAddressEdit->setText(QString());
        ui->webPageLabel->setText(QString());
        ui->loginEdit->setText(QString());
        ui->passwordEdit->setText(QString());

        ui->cameraScheduleWidget->setScheduleTasks(QnScheduleTaskList());
        ui->cameraScheduleWidget->setScheduleEnabled(false);
        ui->cameraScheduleWidget->setChangesDisabled(false); /* Do not block editing by default if schedule task list is empty. */
        ui->cameraScheduleWidget->setCameras(QnVirtualCameraResourceList());

        ui->softwareMotionButton->setEnabled(false);
        ui->cameraMotionButton->setText(QString());
        ui->motionWebPageLabel->setText(QString());
        ui->cameraMotionButton->setChecked(false);
        ui->softwareMotionButton->setChecked(false);

        m_cameraSupportsMotion = false;
        ui->motionSettingsGroupBox->setEnabled(false);
        ui->motionAvailableLabel->setVisible(true);
    } else {
        QString webPageAddress = QString(QLatin1String("http://%1")).arg(m_camera->getHostAddress().toString());

        ui->nameEdit->setText(m_camera->getName());
        ui->checkBoxEnableAudio->setChecked(m_camera->isAudioEnabled());
        ui->checkBoxEnableAudio->setEnabled(m_camera->isAudioSupported());
        ui->macAddressEdit->setText(m_camera->getMAC().toString());
        ui->ipAddressEdit->setText(m_camera->getUrl());
        ui->webPageLabel->setText(tr("<a href=\"%1\">%2</a>").arg(webPageAddress).arg(webPageAddress));
        ui->loginEdit->setText(m_camera->getAuth().user());
        ui->passwordEdit->setText(m_camera->getAuth().password());

        ui->softwareMotionButton->setEnabled(m_camera->supportedMotionType() & MT_SoftwareGrid);
        if (m_camera->supportedMotionType() & (MT_HardwareGrid | MT_MotionWindow))
            ui->cameraMotionButton->setText(tr("Hardware (Camera built-in)"));
        else
            ui->cameraMotionButton->setText(tr("Do not record motion"));

        QnVirtualCameraResourceList cameras;
        cameras.push_back(m_camera);
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
            ui->cameraScheduleWidget->setFps(ui->cameraScheduleWidget->getMaxFps()/2);

        updateMaxFPS();

        ui->motionWebPageLabel->setText(webPageAddress); // TODO: wrong, need to get camera-specific web page
        ui->cameraMotionButton->setChecked(m_camera->getMotionType() != MT_SoftwareGrid);
        ui->softwareMotionButton->setChecked(m_camera->getMotionType() == MT_SoftwareGrid);

        m_cameraSupportsMotion = m_camera->supportedMotionType() != MT_NoMotion;
        ui->motionSettingsGroupBox->setEnabled(m_cameraSupportsMotion);
        ui->motionAvailableLabel->setVisible(!m_cameraSupportsMotion);
    }

    updateMotionWidgetFromResource();
    updateMotionAvailability();

    setHasDbChanges(false);
    setHasCameraChanges(false);
}

void QnSingleCameraSettingsWidget::updateMotionWidgetFromResource() {
    if(!m_motionWidget)
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
    setReadOnly(ui->checkBoxEnableAudio, readOnly);
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
}

void QnSingleCameraSettingsWidget::updateMotionWidgetNeedControlMaxRect() {
    if(!m_motionWidget)
        return;
    bool hwMotion = m_camera && (m_camera->supportedMotionType() & (MT_HardwareGrid | MT_MotionWindow));
    m_motionWidget->setNeedControlMaxRects(m_cameraSupportsMotion && hwMotion && !ui->softwareMotionButton->isChecked());
}

void QnSingleCameraSettingsWidget::updateMotionAvailability() {
    bool motionAvailable = true;
    if(ui->cameraMotionButton->isChecked())
        motionAvailable = m_camera && (m_camera->getCameraBasedMotionType() != MT_NoMotion);

    ui->cameraScheduleWidget->setMotionAvailable(motionAvailable);
}

void QnSingleCameraSettingsWidget::updateMotionWidgetSensitivity() {
    if(m_motionWidget)
        m_motionWidget->setMotionSensitivity(ui->sensitivitySlider->value());
}

bool QnSingleCameraSettingsWidget::isValidMotionRegion(){
    if (!m_motionWidget) 
        return true;
    return m_motionWidget->isValidMotionRegion();
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

void QnSingleCameraSettingsWidget::at_advancedSettingsLoaded(int httpStatusCode, const QList<QPair<QString, QVariant> >& params)
{
    if (!m_widgetsRecreator) {
        qWarning() << "QnSingleCameraSettingsWidget::at_advancedSettingsLoaded: widgets creator ptr is null, camera id: "
            << (m_camera == 0? QString::fromLatin1("unknown"): m_camera->getUniqueId());
        return;
    }
    if (httpStatusCode != 0) {
        qWarning() << "QnSingleCameraSettingsWidget::at_advancedSettingsLoaded: http status code is not OK: " << httpStatusCode
            << ". Camera id: " << (m_camera == 0? QString::fromLatin1("unknown"): m_camera->getUniqueId());
        return;
    }

    if (!m_camera || m_camera->getUniqueId() != m_widgetsRecreator->getCameraId()) {
        //If so, we received update for some other camera
        return;
    }

    bool changesFound = false;
    QList<QPair<QString, QVariant> >::ConstIterator it = params.begin();

    for (; it != params.end(); ++it)
    {
        QString key = it->first;
        QString val = it->second.toString();

        if (!val.isEmpty())
        {
            CameraSettingPtr tmp(new CameraSetting());
            tmp->deserializeFromStr(val);

            CameraSettings::Iterator sIt = m_cameraSettings.find(tmp->getId());
            if (sIt == m_cameraSettings.end()) {
                changesFound = true;
                m_cameraSettings.insert(tmp->getId(), tmp);
                continue;
            }

            CameraSetting& savedVal = *(sIt.value());
            if (CameraSettingReader::isEnabled(savedVal)) {
                CameraSettingValue newVal = tmp->getCurrent();
                if (savedVal.getCurrent() != newVal) {
                    changesFound = true;
                    savedVal.setCurrent(newVal);
                }
                continue;
            }

            if (CameraSettingReader::isEnabled(*tmp)) {
                changesFound = true;
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

void QnSingleCameraSettingsWidget::updateMaxFPS() {
    if (m_inUpdateMaxFps)
        return; /* Do not show message twice. */

    if(!m_camera)
        return; // TODO: investigate why we get here with null camera

    m_inUpdateMaxFps = true;
    if ((ui->softwareMotionButton->isEnabled() &&  ui->softwareMotionButton->isChecked())
        || ui->cameraScheduleWidget->isSecondaryStreamReserver()) {
        float maxFps = m_camera->getMaxFps()-2;
        float currentMaxFps = ui->cameraScheduleWidget->getGridMaxFps();
        if (currentMaxFps > maxFps)
        {
            QMessageBox::warning(this, tr("Warning. FPS value is too high"), 
                tr("For software motion 2 fps is reserved for secondary stream. Current fps in schedule grid is %1. Fps dropped down to %2").arg(currentMaxFps).arg(maxFps));
        }
        ui->cameraScheduleWidget->setMaxFps(maxFps);
    } else {
        ui->cameraScheduleWidget->setMaxFps(m_camera->getMaxFps());
    }
    m_inUpdateMaxFps = false;
}

void QnSingleCameraSettingsWidget::at_motionSelectionCleared() {
    if (m_motionWidget)
        m_motionWidget->clearMotion();
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
    setHasDbChanges(true);
}

void QnSingleCameraSettingsWidget::at_cameraDataChanged() {
    setHasCameraChanges(true);
}

void QnSingleCameraSettingsWidget::at_cameraScheduleWidget_scheduleTasksChanged() {
    at_dbDataChanged();
    at_cameraDataChanged();

    m_hasScheduleChanges = true;
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
