#include "single_camera_settings_widget.h"
#include "ui_single_camera_settings_widget.h"
#include <core/resource/media_server_resource.h>
#include "core/resource/resource_fwd.h"

#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QProcess>
#include <QtWidgets/QMessageBox>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStackedLayout>
#include <QtWidgets/QStyle>

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

#include <utils/common/scoped_value_rollback.h>
#include <utils/license_usage_helper.h>
#include <common/common_module.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include "api/app_server_connection.h"
#include "api/network_proxy_factory.h"
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
    m_hasCameraChanges(false),
    m_anyCameraChanges(false),
    m_hasDbChanges(false),
    m_scheduleEnabledChanged(false),
    m_hasScheduleChanges(false),
    m_hasScheduleControlsChanges(false),
    m_hasMotionControlsChanges(false),
    m_readOnly(false),
    m_updating(false),
    m_motionWidget(NULL),
    m_inUpdateMaxFps(false),
    m_widgetsRecreator(0),
    m_serverConnection(0)
{
    ui->setupUi(this);

    m_motionLayout = new QVBoxLayout(ui->motionWidget);
    m_motionLayout->setContentsMargins(0, 0, 0, 0);

    connect(ui->generalCameraSettingsWidget,   &QnGeneralCameraSettingsWidget::dataChanged,  this,   &QnSingleCameraSettingsWidget::at_dbDataChanged);

    /* Set up context help. */
    setHelpTopic(this,                                                      Qn::CameraSettings_Help);
   
    setHelpTopic(ui->recordingTab,                                          Qn::CameraSettings_Recording_Help);
    setHelpTopic(ui->motionTab,                                             Qn::CameraSettings_Motion_Help);
    setHelpTopic(ui->advancedTab,                                           Qn::CameraSettings_Properties_Help);
    setHelpTopic(ui->fisheyeTab,                                            Qn::CameraSettings_Dewarping_Help);


    connect(ui->tabWidget,              SIGNAL(currentChanged(int)),            this,   SLOT(at_tabWidget_currentChanged()));
    at_tabWidget_currentChanged();



    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleTasksChanged()),         this,   SLOT(at_cameraScheduleWidget_scheduleTasksChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(recordingSettingsChanged()),     this,   SLOT(at_cameraScheduleWidget_recordingSettingsChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(gridParamsChanged()),            this,   SLOT(at_cameraScheduleWidget_gridParamsChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(controlsChangesApplied()),       this,   SLOT(at_cameraScheduleWidget_controlsChangesApplied()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleEnabledChanged(int)),    this,   SLOT(at_cameraScheduleWidget_scheduleEnabledChanged()));

    connect(ui->cameraScheduleWidget,   SIGNAL(gridParamsChanged()),            this,   SLOT(updateMaxFPS()));
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleEnabledChanged(int)),    this,   SLOT(at_dbDataChanged()));
    connect(ui->cameraScheduleWidget,   SIGNAL(archiveRangeChanged()),          this,   SLOT(at_dbDataChanged()));
    connect(ui->cameraScheduleWidget,   &QnCameraScheduleWidget::moreLicensesRequested, this, [this]{menu()->trigger(Qn::PreferencesLicensesTabAction);});
    connect(ui->cameraScheduleWidget,   SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)), this, SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)));
    
    connect(ui->motionWebPageLabel,     SIGNAL(linkActivated(const QString &)), this,   SLOT(at_linkActivated(const QString &)));

    connect(ui->cameraMotionButton,     SIGNAL(clicked(bool)),                  this,   SLOT(at_motionTypeChanged()));
    connect(ui->softwareMotionButton,   SIGNAL(clicked(bool)),                  this,   SLOT(at_motionTypeChanged()));
    connect(ui->sensitivitySlider,      SIGNAL(valueChanged(int)),              this,   SLOT(updateMotionWidgetSensitivity()));
    connect(ui->resetMotionRegionsButton, SIGNAL(clicked()),                    this,   SLOT(at_motionSelectionCleared()));


    connect(ui->expertSettingsWidget,   SIGNAL(dataChanged()),                  this,   SLOT(at_dbDataChanged()));
    connect(ui->expertSettingsWidget,   SIGNAL(secondStreamDisabled(bool)),    ui->cameraScheduleWidget,    SLOT(onSecondStreamStateChange(bool)));
    connect(ui->expertSettingsWidget,   SIGNAL(secondStreamInitialState(bool)),ui->cameraScheduleWidget,    SLOT(updateMotionButtons(bool)));

    connect(ui->generalCameraSettingsWidget,    &QnGeneralCameraSettingsWidget::fisheyeSettingsChanged, this, &QnSingleCameraSettingsWidget::at_fisheyeSettingsChanged);
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

void QnSingleCameraSettingsWidget::at_authenticationRequired(QNetworkReply* /*reply*/, QAuthenticator * authenticator)
{
    QMutexLocker locker(&m_cameraMutex);
    if (!m_camera)
        return;
    authenticator->setUser(m_camera->getAuth().user());
    authenticator->setPassword(m_camera->getAuth().password());
}

void QnSingleCameraSettingsWidget::at_proxyAuthenticationRequired ( const QNetworkProxy & , QAuthenticator * authenticator)
{    
    QMutexLocker locker(&m_cameraMutex);
    if (!m_camera)
        return;

    QString user = QnAppServerConnectionFactory::url().userName();
    QString password = QnAppServerConnectionFactory::url().password();
    authenticator->setUser(user);
    authenticator->setPassword(password);
}

#ifdef QT_WEBKITWIDGETS_LIB
void QnSingleCameraSettingsWidget::updateWebPage(QStackedLayout* stackedLayout , QWebView* advancedWebView)
{
    if ( qnCommon )
    {
        QnResourceData resourceData = qnCommon->dataPool()->data(m_camera);
        bool showUrl = resourceData.value<bool>(lit("showUrl"), false);
        if ( showUrl && m_camera->getStatus() != QnResource::Offline )
        {
            QnNetworkProxyFactory::instance()->removeFromProxyList(m_lastCameraPageUrl.host());
            
            m_lastCameraPageUrl = QString(QLatin1String("http://%1:%2/%3")).
                arg(m_camera->getHostAddress()).arg(m_camera->httpPort()).
                arg(resourceData.value<QString>(lit("urlLocalePath"), QString()));
            m_lastCameraPageUrl.setUserName( m_camera->getAuth().user() );
            m_lastCameraPageUrl.setPassword( m_camera->getAuth().password() );

            QnNetworkProxyFactory::instance()->bindHostToResource( m_lastCameraPageUrl.host(), m_camera );
            advancedWebView->reload();
            advancedWebView->load( QNetworkRequest(m_lastCameraPageUrl) );
            advancedWebView->show();
            stackedLayout->setCurrentIndex(1);
        }
        else
        {
            stackedLayout->setCurrentIndex(0);
        }                
    } 
}
#endif

bool QnSingleCameraSettingsWidget::initAdvancedTab()
{
    QVariant id;
    QTreeWidget* advancedTreeWidget = 0;
    QStackedLayout* advancedLayout = 0;
#ifdef QT_WEBKITWIDGETS_LIB
    QWebView* advancedWebView = 0;
#endif
    setAnyCameraChanges(false);
    bool isCameraSettingsId = false;
    bool showUrl = false;
    
    if ( m_camera )
    {
        m_camera->getParam(lit("cameraSettingsId"), id, QnDomainDatabase);
        isCameraSettingsId = !id.isNull();
        
        if ( qnCommon && qnCommon->dataPool() )
        {
            QnResourceData resourceData = qnCommon->dataPool()->data(m_camera);
            showUrl = resourceData.value<bool>(lit("showUrl"), false);
        };
    }
	bool showOldSettings = (isCameraSettingsId && !showUrl);
    if (isCameraSettingsId || showUrl)
    {
        if (!m_widgetsRecreator)
        {
            QStackedLayout* stacked_layout = dynamic_cast<QStackedLayout*>(ui->advancedTab->layout());
            if(!stacked_layout) 
            {
                delete ui->advancedTab->layout();
                ui->advancedTab->setLayout(stacked_layout = new QStackedLayout());
            }

            QSplitter* advancedSplitter = new QSplitter();
            advancedSplitter->setChildrenCollapsible(false);

            stacked_layout->addWidget(advancedSplitter);

            advancedTreeWidget = new QTreeWidget();
            advancedTreeWidget->setColumnCount(1);
            advancedTreeWidget->setHeaderLabel(lit("Category")); // TODO: #TR #Elric

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
#ifdef QT_WEBKITWIDGETS_LIB
            advancedWebView = new QWebView(ui->advancedTab);

            QStyle* style = QStyleFactory().create(lit("fusion"));
            advancedWebView->setStyle(style);


            QPalette palGreenHlText = this->palette();
                
            // Outline around the menu
            palGreenHlText.setColor(QPalette::Window, Qt::gray);    
            palGreenHlText.setColor(QPalette::WindowText, Qt::black);

            palGreenHlText.setColor(QPalette::BrightText, Qt::gray);  
            palGreenHlText.setColor(QPalette::BrightText, Qt::black);

            // combo button
            palGreenHlText.setColor(QPalette::Button, Qt::gray);  
            palGreenHlText.setColor(QPalette::ButtonText, Qt::black);

            // combo menu
            palGreenHlText.setColor(QPalette::Base, Qt::gray);  
            palGreenHlText.setColor(QPalette::Text, Qt::black);

            // tool tips
            palGreenHlText.setColor(QPalette::ToolTipBase, Qt::gray);  
            palGreenHlText.setColor(QPalette::ToolTipText, Qt::black);

            palGreenHlText.setColor(QPalette::NoRole, Qt::gray);  
            palGreenHlText.setColor(QPalette::AlternateBase, Qt::gray);  

            palGreenHlText.setColor(QPalette::Link, Qt::black);  
            palGreenHlText.setColor(QPalette::LinkVisited, Qt::black);  

            

            // highlight button & menu
            palGreenHlText.setColor(QPalette::Highlight, Qt::gray);   
            palGreenHlText.setColor(QPalette::HighlightedText, Qt::black);

            // to customize the disabled color
            palGreenHlText.setColor(QPalette::Disabled, QPalette::Button, Qt::gray);
            palGreenHlText.setColor(QPalette::Disabled, QPalette::ButtonText, Qt::black);



            advancedWebView->setPalette(palGreenHlText);
            
            advancedWebView->setAutoFillBackground(true);
            //advancedWebView->setStyleSheet(lit("background-color: rgb(255, 255, 255); color: rgb(255, 255, 255)"));
            
            advancedWebView->setBackgroundRole(palGreenHlText.Base);
            advancedWebView->setForegroundRole(palGreenHlText.Base);
                
            

            connect(advancedWebView->page()->networkAccessManager(), &QNetworkAccessManager::sslErrors,
                this, [](QNetworkReply* reply, const QList<QSslError> &){reply->ignoreSslErrors();} );
            connect(advancedWebView->page()->networkAccessManager(), &QNetworkAccessManager::authenticationRequired,
                    this, &QnSingleCameraSettingsWidget::at_authenticationRequired, Qt::DirectConnection );
            connect(advancedWebView->page()->networkAccessManager(), &QNetworkAccessManager::proxyAuthenticationRequired,
                    this, &QnSingleCameraSettingsWidget::at_proxyAuthenticationRequired, Qt::DirectConnection);
           
            stacked_layout->addWidget(advancedWebView);
            if( currentTab() == Qn::AdvancedCameraSettingsTab )
                updateWebPage(stacked_layout, advancedWebView);
#endif
        } else {
            if (m_camera->getUniqueId() == m_widgetsRecreator->getCameraId()) {
                return showOldSettings;
            }

            if (id == m_widgetsRecreator->getId()) {

                //ToDo: disable all and return. Currently, will do the same as for another type of cameras
                //disable;
                //return;
            }

            advancedTreeWidget = m_widgetsRecreator->getRootWidget();
            advancedLayout = m_widgetsRecreator->getRootLayout();
#ifdef QT_WEBKITWIDGETS_LIB
            advancedWebView = m_widgetsRecreator->getWebView();
#endif
            cleanAdvancedSettings();
        }

        m_widgetsRecreator = new CameraSettingsWidgetsTreeCreator(m_camera->getUniqueId(), id.toString(), 
#ifdef QT_WEBKITWIDGETS_LIB
            advancedWebView,
#endif
            *advancedTreeWidget, *advancedLayout);
        connect(m_widgetsRecreator, &CameraSettingsWidgetsTreeCreator::advancedParamChanged, this, &QnSingleCameraSettingsWidget::at_advancedParamChanged);
        connect(m_widgetsRecreator, &CameraSettingsWidgetsTreeCreator::refreshAdvancedSettings, this, &QnSingleCameraSettingsWidget::refreshAdvancedSettings, Qt::QueuedConnection);

    }
    else if (m_widgetsRecreator)
    {
        advancedTreeWidget = m_widgetsRecreator->getRootWidget();
        advancedLayout = m_widgetsRecreator->getRootLayout();
#ifdef QT_WEBKITWIDGETS_LIB
        advancedWebView = m_widgetsRecreator->getWebView();
#endif
        cleanAdvancedSettings();

        //Dummy creator: required for cameras, that doesn't support advanced settings
		m_widgetsRecreator = new CameraSettingsWidgetsTreeCreator(QString(), QString(),
#ifdef QT_WEBKITWIDGETS_LIB
            advancedWebView,
#endif
            *advancedTreeWidget, *advancedLayout);
    }

    return showOldSettings;
}

void QnSingleCameraSettingsWidget::cleanAdvancedSettings()
{
    if (m_widgetsRecreator) {
        disconnect(m_widgetsRecreator, NULL, this, NULL);
        delete m_widgetsRecreator;
        m_widgetsRecreator = NULL;
    }
    m_cameraSettings.clear();
}

void QnSingleCameraSettingsWidget::loadAdvancedSettings()
{
	bool showOldSettings = initAdvancedTab();
    if ( showOldSettings )
    {
        QVariant id;
        if (m_camera && m_camera->getParam(lit("cameraSettingsId"), id, QnDomainDatabase) && !id.isNull())
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
    
}

const QnVirtualCameraResourcePtr &QnSingleCameraSettingsWidget::camera() const {
    return m_camera;
}

void QnSingleCameraSettingsWidget::setCamera(const QnVirtualCameraResourcePtr &camera) {
    Q_D(QnCameraSettingsWidget);

    if(m_camera == camera)
        return;

    QMutexLocker locker(&m_cameraMutex);
    m_camera = camera;
    d->setCameras(QnVirtualCameraResourceList() << camera);

    ui->generalCameraSettingsWidget->setCameras(QnVirtualCameraResourceList() << camera);

    if(m_camera) {
      
#ifdef QT_WEBKITWIDGETS_LIB
        QStackedLayout* stacked_layout = dynamic_cast<QStackedLayout*>(ui->advancedTab->layout());
        if ( m_widgetsRecreator && m_widgetsRecreator->getWebView() && stacked_layout && (currentTab() == Qn::AdvancedCameraSettingsTab) )
            updateWebPage( stacked_layout, m_widgetsRecreator->getWebView() );
#endif
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

    if (hasDbChanges()) {
        ui->generalCameraSettingsWidget->submitToResources();

        if (!m_camera->isDtsBased())
            m_camera->setScheduleDisabled(!ui->cameraScheduleWidget->isScheduleEnabled());

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



        ui->expertSettingsWidget->submitToResources(QnVirtualCameraResourceList() << m_camera);

        QnMediaDewarpingParams dewarpingParams = m_camera->getDewarpingParams();
        ui->fisheyeSettingsWidget->submitToParams(dewarpingParams);
        m_camera->setDewarpingParams(dewarpingParams);

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
    updateFromResource();
}

bool QnSingleCameraSettingsWidget::licensedParametersModified() const
{
    if( !hasDbChanges() && !hasCameraChanges() && !hasAnyCameraChanges() )
        return false;//nothing have been changed

    return m_scheduleEnabledChanged || m_hasScheduleChanges;
}

void QnSingleCameraSettingsWidget::updateFromResource() {
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    ui->generalCameraSettingsWidget->updateFromResources();
    loadAdvancedSettings();

    {
        bool dtsBased = m_camera->isDtsBased();
        ui->tabWidget->setTabEnabled(Qn::RecordingSettingsTab, !dtsBased);
        ui->tabWidget->setTabEnabled(Qn::MotionSettingsTab, !dtsBased);
        ui->tabWidget->setTabEnabled(Qn::AdvancedCameraSettingsTab, !dtsBased);
        ui->tabWidget->setTabEnabled(Qn::ExpertCameraSettingsTab, !dtsBased);


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

            m_cameraSupportsMotion = m_camera->hasMotion();
            ui->motionSettingsGroupBox->setEnabled(m_cameraSupportsMotion);
            ui->motionAvailableLabel->setVisible(!m_cameraSupportsMotion);

            ui->cameraScheduleWidget->endUpdate(); //here gridParamsChanged() can be called that is connected to updateMaxFps() method

            ui->expertSettingsWidget->updateFromResources(QnVirtualCameraResourceList() << m_camera);

            if (!m_imageProvidersByResourceId.contains(m_camera->getId()))
                m_imageProvidersByResourceId[m_camera->getId()] = QnSingleThumbnailLoader::newInstance(m_camera, -1, fisheyeThumbnailSize, QnSingleThumbnailLoader::JpgFormat, this);
            ui->fisheyeSettingsWidget->updateFromParams(m_camera->getDewarpingParams(), m_imageProvidersByResourceId[m_camera->getId()]);
        }
    }

    ui->tabWidget->setTabEnabled(Qn::FisheyeCameraSettingsTab, ui->fisheyeCheckBox->isChecked());

    updateMotionWidgetFromResource();
    updateMotionAvailability();
    updateLicenseText();
    updateRecordingParamsAvailability();

    setHasDbChanges(false);
    setHasCameraChanges(false);
    m_scheduleEnabledChanged = false;
    m_hasScheduleControlsChanges = false;
    m_hasMotionControlsChanges = false;

    if (m_camera)
        updateMaxFPS();

    // Rollback the fisheye preview options. Makes no changes if params were not modified. --gdm
    QnResourceWidget* centralWidget = display()->widget(Qn::CentralRole);
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

    m_camera->setMotionRegionList(m_motionWidget->motionRegionList(), QnDomainMemory);
}

bool QnSingleCameraSettingsWidget::isReadOnly() const {
    return m_readOnly;
}

void QnSingleCameraSettingsWidget::setReadOnly(bool readOnly) {
    if(m_readOnly == readOnly)
        return;

    using ::setReadOnly;
    setReadOnly(ui->generalCameraSettingsWidget, readOnly);
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
    {
        m_scheduleEnabledChanged = false;
        m_hasScheduleChanges = false;
    }

    emit hasChangesChanged();
}

void QnSingleCameraSettingsWidget::setHasCameraChanges(bool hasChanges) {
    if(m_hasCameraChanges == hasChanges)
        return;

    m_hasCameraChanges = hasChanges;
    if(!m_hasCameraChanges && !hasDbChanges())
    {
        m_scheduleEnabledChanged = false;
        m_hasScheduleChanges = false;
    }

    emit hasChangesChanged();
}

void QnSingleCameraSettingsWidget::setAnyCameraChanges(bool hasChanges) {
    if(m_anyCameraChanges == hasChanges)
        return;

    m_anyCameraChanges = hasChanges;
    if(!m_anyCameraChanges && !hasDbChanges())
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
    
    ui->cameraScheduleWidget->setRecordingParamsAvailability(!m_camera->hasParam(lit("noRecordingParams")));
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
            << (m_camera == 0? lit("unknown"): m_camera->getUniqueId());
        return;
    }
    if (status != 0) {
        qWarning() << "QnSingleCameraSettingsWidget::at_advancedSettingsLoaded: http status code is not OK: " << status
            << ". Camera id: " << (m_camera == 0? lit("unknown"): m_camera->getUniqueId());
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

void QnSingleCameraSettingsWidget::updateLicenseText() {
    if (!m_camera || !m_camera->isDtsBased())
        return;
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
    QAuthenticator auth = ui->generalCameraSettingsWidget->authenticator();
    url.setUserName(auth.user());
    url.setPassword(auth.password());
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

#ifdef QT_WEBKITWIDGETS_LIB
        case Qn::AdvancedCameraSettingsTab:
        {
            QStackedLayout* stacked_layout = dynamic_cast<QStackedLayout*>(ui->advancedTab->layout());
            if ( m_widgetsRecreator && m_widgetsRecreator->getWebView() && stacked_layout && (currentTab() == Qn::AdvancedCameraSettingsTab) )
                updateWebPage( stacked_layout, m_widgetsRecreator->getWebView() );
            break;
        }
#endif

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

    ui->tabWidget->setTabEnabled(Qn::FisheyeCameraSettingsTab, ui->fisheyeCheckBox->isChecked());
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
    ui->analogViewCheckBox->setChecked(ui->cameraScheduleWidget->isScheduleEnabled());
    m_scheduleEnabledChanged = true;
}

void QnSingleCameraSettingsWidget::at_advancedParamChanged(const CameraSetting& val) {
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

void QnSingleCameraSettingsWidget::at_fisheyeSettingsChanged() {
    if (m_updating)
        return;

    at_dbDataChanged();
    at_cameraDataChanged();

    // Preview the changes on the central widget

    QnResourceWidget* centralWidget = display()->widget(Qn::CentralRole);
    if (!m_camera || !centralWidget || centralWidget->resource() != m_camera)
        return;

    if (QnMediaResourceWidget* mediaWidget = dynamic_cast<QnMediaResourceWidget*>(centralWidget)) {
        QnMediaDewarpingParams dewarpingParams = mediaWidget->dewarpingParams();
        ui->fisheyeSettingsWidget->submitToParams(dewarpingParams);
        dewarpingParams.enabled = ui->fisheyeCheckBox->isChecked();
        mediaWidget->setDewarpingParams(dewarpingParams);

        QnWorkbenchItem *item = mediaWidget->item();
        QnItemDewarpingParams itemParams = item->dewarpingParams();
        itemParams.enabled = dewarpingParams.enabled;
        item->setDewarpingParams(itemParams);
    }
}

void QnSingleCameraSettingsWidget::setGridScheduleToAlwaysRecord() {
    ui->cameraScheduleWidget->resetGridWidget();
    at_cameraScheduleWidget_scheduleTasksChanged();
}
