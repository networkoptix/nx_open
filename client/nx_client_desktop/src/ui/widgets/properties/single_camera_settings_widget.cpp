#include "single_camera_settings_widget.h"
#include "ui_single_camera_settings_widget.h"

#include <QtCore/QBuffer>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QProcess>
#include <QtCore/QScopedValueRollback>
#include <QtGui/QDesktopServices>

#include <camera/single_thumbnail_loader.h>
#include <camera/camera_thumbnail_manager.h>
#include <camera/fps_calculator.h>

// TODO: #GDM #Common ask: what about constant MIN_SECOND_STREAM_FPS moving out of this module
#include <core/dataprovider/live_stream_provider.h>
#include <core/resource/resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/core/utils/geometry.h>
#include <ui/common/aligner.h>
#include <ui/common/read_only.h>

#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/custom_style.h>

#include <ui/widgets/common/selectable_button.h>
#include <ui/widgets/properties/camera_schedule_widget.h>
#include <ui/widgets/properties/camera_motion_mask_widget.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>

#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/license_usage_helper.h>
#include <utils/common/delayed.h>

#include "client/client_settings.h"

using namespace nx::client::desktop::ui;
using nx::client::core::Geometry;

namespace {

static const QSize kFisheyeThumbnailSize(0, 0); //unlimited size for better calibration
static const QSize kSensitivityButtonSize(34, 34);
static const QString kRemoveCredentialsFromWebPageUrl = lit("removeCredentialsFromWebPageUrl");

} // unnamed namespace


QnSingleCameraSettingsWidget::QnSingleCameraSettingsWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::SingleCameraSettingsWidget),
    m_cameraSupportsMotion(false),
    m_hasDbChanges(false),
    m_hasMotionControlsChanges(false),
    m_readOnly(false),
    m_updating(false),
    m_motionWidget(NULL),
    m_sensitivityButtons(new QButtonGroup(this))
{
    ui->setupUi(this);
    ui->licensingWidget->initializeContext(this);
    ui->cameraScheduleWidget->initializeContext(this);
    ui->motionDetectionCheckBox->setProperty(style::Properties::kCheckBoxAsButton, true);
    ui->motionDetectionCheckBox->setForegroundRole(QPalette::ButtonText);

    for (int i = 0; i < QnMotionRegion::kSensitivityLevelCount; ++i)
    {
        auto button = new QnSelectableButton(ui->motionSensitivityGroupBox);
        button->setText(lit("%1").arg(i));
        button->setFixedSize(kSensitivityButtonSize);
        ui->motionSensitivityGroupBox->layout()->addWidget(button);
        m_sensitivityButtons->addButton(button, i);
    }

    m_sensitivityButtons->setExclusive(true);
    m_sensitivityButtons->button(0)->setChecked(true);

    m_motionLayout = new QVBoxLayout(ui->motionWidget);
    m_motionLayout->setContentsMargins(0, 0, 0, 0);

    /* Set up context help. */
    setHelpTopic(this, Qn::CameraSettings_Help);

    setHelpTopic(ui->nameLabel, ui->nameEdit, Qn::CameraSettings_General_Name_Help);
    setHelpTopic(ui->modelLabel, ui->modelEdit, Qn::CameraSettings_General_Model_Help);
    setHelpTopic(ui->firmwareLabel, ui->firmwareEdit, Qn::CameraSettings_General_Firmware_Help);
    // TODO: #Elric add context help for vendor field
    setHelpTopic(ui->addressGroupBox, Qn::CameraSettings_General_Address_Help);
    setHelpTopic(ui->enableAudioCheckBox, Qn::CameraSettings_General_Audio_Help);
    setHelpTopic(ui->authenticationGroupBox, Qn::CameraSettings_General_Auth_Help);
    setHelpTopic(ui->recordingTab, Qn::CameraSettings_Recording_Help);
    setHelpTopic(ui->motionTab, Qn::CameraSettings_Motion_Help);
    setHelpTopic(ui->advancedTab, Qn::CameraSettings_Properties_Help);
    setHelpTopic(ui->fisheyeTab, Qn::CameraSettings_Dewarping_Help);

    connect(ui->tabWidget, &QTabWidget::currentChanged,
        this, &QnSingleCameraSettingsWidget::at_tabWidget_currentChanged);
    at_tabWidget_currentChanged();

    connect(ui->nameEdit, &QLineEdit::textChanged,
        this, &QnSingleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->enableAudioCheckBox, &QCheckBox::stateChanged,
        this, &QnSingleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->loginEdit, &QLineEdit::textChanged,
        this, &QnSingleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->passwordEdit, &QLineEdit::textChanged,
        this, &QnSingleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->cameraScheduleWidget, &QnCameraScheduleWidget::scheduleTasksChanged,
        this, &QnSingleCameraSettingsWidget::at_dbDataChanged);
    connect(ui->cameraScheduleWidget, &QnCameraScheduleWidget::recordingSettingsChanged,
        this, &QnSingleCameraSettingsWidget::at_dbDataChanged);
    connect(ui->cameraScheduleWidget, &QnCameraScheduleWidget::scheduleEnabledChanged,
        this, &QnSingleCameraSettingsWidget::at_cameraScheduleWidget_scheduleEnabledChanged);
    connect(ui->cameraScheduleWidget, &QnCameraScheduleWidget::scheduleEnabledChanged,
        this, &QnSingleCameraSettingsWidget::at_dbDataChanged);
    connect(ui->cameraScheduleWidget, &QnCameraScheduleWidget::archiveRangeChanged,
        this, &QnSingleCameraSettingsWidget::at_dbDataChanged);
    connect(ui->cameraScheduleWidget, &QnCameraScheduleWidget::alert, this,
        [this](const QString& text) { m_recordingAlert = text; updateAlertBar(); });

    connect(ui->webPageLink, &QLabel::linkActivated,
        this, &QnSingleCameraSettingsWidget::at_linkActivated);

    connect(ui->motionDetectionCheckBox, &QCheckBox::toggled, this,
        [this]
        {
            updateMotionAlert();
            at_motionTypeChanged();
        });

    connect(m_sensitivityButtons, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked),
        this, &QnSingleCameraSettingsWidget::updateMotionWidgetSensitivity);

    connect(ui->resetMotionRegionsButton, &QPushButton::clicked,
        this, &QnSingleCameraSettingsWidget::at_resetMotionRegionsButton_clicked);

    connect(ui->pingButton, &QPushButton::clicked, this,
        [this]
        {
            /* We must always ping the same address that is displayed in the visible field. */
            auto host = ui->ipAddressEdit->text();
            menu()->trigger(action::PingAction, {Qn::TextRole, host});
        });

    connect(ui->licensingWidget, &QnLicensesProposeWidget::changed,
        this, &QnSingleCameraSettingsWidget::at_dbDataChanged);
    connect(ui->licensingWidget, &QnLicensesProposeWidget::changed, this,
        [this]
        {
            if (m_updating)
                return;

            ui->cameraScheduleWidget->setScheduleEnabled(ui->licensingWidget->state() == Qt::Checked);
        });

    connect(ui->expertSettingsWidget, &QnCameraExpertSettingsWidget::dataChanged,
        this, &QnSingleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->fisheyeSettingsWidget, &QnFisheyeSettingsWidget::dataChanged,
        this, &QnSingleCameraSettingsWidget::at_fisheyeSettingsChanged);

    connect(ui->imageControlWidget, &QnImageControlWidget::changed,
        this, &QnSingleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->ioPortSettingsWidget, &QnIOPortSettingsWidget::dataChanged,
        this, &QnSingleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->advancedSettingsWidget, &QnCameraAdvancedSettingsWidget::hasChangesChanged,
        this, &QnSingleCameraSettingsWidget::hasChangesChanged);

    updateFromResource(true);
    retranslateUi();

    QnAligner* aligner = new QnAligner(this);
    aligner->addWidgets({
        ui->nameLabel,
        ui->modelLabel,
        ui->firmwareLabel,
        ui->vendorLabel,
        ui->ipAddressLabel,
        ui->webPageLabel,
        ui->macAddressLabel,
        ui->loginLabel,
        ui->passwordLabel });
}

QnSingleCameraSettingsWidget::~QnSingleCameraSettingsWidget()
{
}

void QnSingleCameraSettingsWidget::retranslateUi()
{
    setWindowTitle(QnDeviceDependentStrings::getNameFromSet(
        resourcePool(),
        QnCameraDeviceStringSet(
            tr("Device Settings"),
            tr("Camera Settings"),
            tr("I/O Module Settings")
        ), m_camera
    ));
}

const QnVirtualCameraResourcePtr &QnSingleCameraSettingsWidget::camera() const
{
    return m_camera;
}

void QnSingleCameraSettingsWidget::setCamera(const QnVirtualCameraResourcePtr &camera)
{
    if (m_camera == camera)
        return;

    if (m_camera)
    {
        disconnect(m_camera, NULL, this, NULL);
    }

    m_camera = camera;
    QnVirtualCameraResourceList cameras;
    if (m_camera)
        cameras << m_camera;
    ui->advancedSettingsWidget->setCamera(camera);
    ui->cameraScheduleWidget->setCameras(cameras);
    ui->licensingWidget->setCameras(cameras);

    if (m_camera)
    {
        connect(m_camera, &QnResource::urlChanged,      this, &QnSingleCameraSettingsWidget::updateIpAddressText);
        connect(m_camera, &QnResource::resourceChanged, this, &QnSingleCameraSettingsWidget::updateIpAddressText);
        connect(m_camera, &QnResource::urlChanged,      this, &QnSingleCameraSettingsWidget::updateWebPageText); // TODO: #GDM also listen to hostAddress changes?
        connect(m_camera, &QnResource::resourceChanged, this, &QnSingleCameraSettingsWidget::updateWebPageText); // TODO: #GDM why?
        connect(m_camera, &QnResource::resourceChanged, this, &QnSingleCameraSettingsWidget::updateMotionCapabilities);
    }

    updateFromResource(!isVisible());
    if (currentTab() == Qn::AdvancedCameraSettingsTab)
        ui->advancedSettingsWidget->reloadData();
    retranslateUi();
}

Qn::CameraSettingsTab QnSingleCameraSettingsWidget::currentTab() const
{
/* Using field names here so that changes in UI file will lead to compilation errors. */

    QWidget *tab = ui->tabWidget->currentWidget();

    if (tab == ui->generalTab)
        return Qn::GeneralSettingsTab;

    if (tab == ui->recordingTab)
        return Qn::RecordingSettingsTab;

    if (tab == ui->ioSettingsTab)
        return Qn::IOPortsSettingsTab;

    if (tab == ui->motionTab)
        return Qn::MotionSettingsTab;

    if (tab == ui->advancedTab)
        return Qn::AdvancedCameraSettingsTab;

    if (tab == ui->expertTab)
        return Qn::ExpertCameraSettingsTab;

    if (tab == ui->fisheyeTab)
        return Qn::FisheyeCameraSettingsTab;

    qnWarning("Current tab with index %1 was not recognized.", ui->tabWidget->currentIndex());
    return Qn::GeneralSettingsTab;
}

void QnSingleCameraSettingsWidget::setCurrentTab(Qn::CameraSettingsTab tab)
{
/* Using field names here so that changes in UI file will lead to compilation errors. */

    if (!ui->tabWidget->isTabEnabled(tabIndex(tab)))
    {
        ui->tabWidget->setCurrentWidget(ui->generalTab);
        return;
    }

    switch (tab)
    {
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
        case Qn::IOPortsSettingsTab:
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

bool QnSingleCameraSettingsWidget::hasDbChanges() const
{
    return m_hasDbChanges;
}

bool QnSingleCameraSettingsWidget::hasAdvancedCameraChanges() const
{
    return ui->advancedSettingsWidget->hasChanges();
}

bool QnSingleCameraSettingsWidget::hasChanges() const
{
    return hasDbChanges() || hasAdvancedCameraChanges();
}

Qn::MotionType QnSingleCameraSettingsWidget::selectedMotionType() const
{
    if (!m_camera)
        return Qn::MT_Default;

    if (!ui->motionDetectionCheckBox->isChecked())
        return Qn::MT_NoMotion;

    return m_camera->getDefaultMotionType();
}

void QnSingleCameraSettingsWidget::submitToResource()
{
    if (!m_camera)
        return;

    if (isReadOnly())
        return;

    if (hasDbChanges())
    {
        QString name = ui->nameEdit->text().trimmed();
        if (!name.isEmpty())
            m_camera->setName(name);  // TODO: #GDM warning message should be displayed on nameEdit textChanged, Ok/Apply buttons should be blocked.
        m_camera->setAudioEnabled(ui->enableAudioCheckBox->isChecked());

        if (ui->passwordEdit->hasFocus() && ui->passwordEdit->echoMode() == QLineEdit::PasswordEchoOnEdit)
            setFocus();

        QAuthenticator loginEditAuth;
        loginEditAuth.setUser(ui->loginEdit->text().trimmed());
        loginEditAuth.setPassword(ui->passwordEdit->text().trimmed());
        if (m_camera->getAuth() != loginEditAuth)
            m_camera->setAuth(loginEditAuth);

        ui->cameraScheduleWidget->submitToResources();

        if (!m_camera->isDtsBased())
        {
            m_camera->setMotionType(selectedMotionType());
            submitMotionWidgetToResource();
        }

        ui->imageControlWidget->submitToResources(QnVirtualCameraResourceList() << m_camera);
        ui->expertSettingsWidget->submitToResources(QnVirtualCameraResourceList() << m_camera);
        ui->ioPortSettingsWidget->submitToResource(m_camera);

        QnMediaDewarpingParams dewarpingParams = m_camera->getDewarpingParams();
        ui->fisheyeSettingsWidget->submitToParams(dewarpingParams);
        m_camera->setDewarpingParams(dewarpingParams);

        setHasDbChanges(false);
    }

    if (hasAdvancedCameraChanges())
        ui->advancedSettingsWidget->submitToResource();
}

void QnSingleCameraSettingsWidget::updateFromResource(bool silent)
{
    QScopedValueRollback<bool> updateRollback(m_updating, true);

    QnVirtualCameraResourceList cameras;
    if (m_camera)
        cameras << m_camera;

    ui->imageControlWidget->updateFromResources(cameras);

    if (!m_camera)
    {
        ui->nameEdit->clear();
        ui->modelEdit->clear();
        ui->firmwareEdit->clear();
        ui->vendorEdit->clear();
        ui->enableAudioCheckBox->setChecked(false);
        ui->macAddressEdit->clear();
        ui->loginEdit->clear();
        ui->passwordEdit->clear();

        ui->cameraScheduleWidget->setCameras(QnVirtualCameraResourceList());

        m_cameraSupportsMotion = false;
        ui->motionSensitivityGroupBox->setEnabled(false);
        ui->motionControlsWidget->setVisible(false);
        ui->motionAvailableLabel->setVisible(true);
    }
    else
    {
        bool hasVideo = m_camera->hasVideo(0);
        bool hasAudio = m_camera->isAudioSupported();

        ui->nameEdit->setText(m_camera->getName());
        ui->modelEdit->setText(m_camera->getModel());
        ui->firmwareEdit->setText(m_camera->getFirmware());
        ui->vendorEdit->setText(m_camera->getVendor());
        ui->enableAudioCheckBox->setChecked(m_camera->isAudioEnabled());
        ui->enableAudioCheckBox->setEnabled(hasAudio && !m_camera->isAudioForced());

        ui->macAddressEdit->setText(m_camera->getMAC().toString());

        QAuthenticator auth = m_camera->getAuth();

        ui->loginEdit->setText(auth.user());
        ui->passwordEdit->setText(auth.password());

        const bool dtsBased = m_camera->isDtsBased();
        const bool isIoModule = m_camera->isIOModule();
        setTabEnabledSafe(Qn::RecordingSettingsTab, !dtsBased && (hasAudio || hasVideo));
        setTabEnabledSafe(Qn::MotionSettingsTab, !dtsBased && hasVideo);
        setTabEnabledSafe(Qn::ExpertCameraSettingsTab, !dtsBased && hasVideo && !isReadOnly());
        setTabEnabledSafe(Qn::IOPortsSettingsTab, isIoModule);
        setTabEnabledSafe(Qn::FisheyeCameraSettingsTab, !isIoModule);


        if (!dtsBased)
        {
            auto supported = m_camera->supportedMotionType();
            auto motionType = m_camera->getMotionType();
            auto mdEnabled = supported.testFlag(motionType) && motionType != Qn::MT_NoMotion;
            if (!mdEnabled)
                motionType = Qn::MT_NoMotion;
            ui->motionDetectionCheckBox->setChecked(mdEnabled);

            ui->cameraScheduleWidget->overrideMotionType(motionType);

            updateMotionCapabilities();
            updateMotionWidgetFromResource();

            QnVirtualCameraResourceList cameras;
            cameras.push_back(m_camera);
            ui->expertSettingsWidget->updateFromResources(cameras);

            if (!m_imageProvidersByResourceId.contains(m_camera->getId()))
                m_imageProvidersByResourceId[m_camera->getId()] = new QnSingleThumbnailLoader(
                    m_camera,
                    -1,
                    -1,
                    kFisheyeThumbnailSize,
                    QnThumbnailRequestData::JpgFormat,
                    this);
            ui->fisheyeSettingsWidget->updateFromParams(m_camera->getDewarpingParams(), m_imageProvidersByResourceId[m_camera->getId()]);
        }
    }

    /* After overrideMotionType is set. */
    ui->cameraScheduleWidget->updateFromResources();

    updateIpAddressText();
    updateWebPageText();
    ui->advancedSettingsWidget->updateFromResource();
    ui->ioPortSettingsWidget->updateFromResource(m_camera);
    ui->licensingWidget->updateFromResources();

    setHasDbChanges(false);
    m_hasMotionControlsChanges = false;

    m_recordingAlert = QString();
    updateMotionAlert();

    if (m_camera)
    {
        /* Check if schedule was changed during load, e.g. limited by max fps. */
        if (!silent)
        {
            const auto callback = [this]() { showMaxFpsWarningIfNeeded(); };
            executeDelayedParented(callback, kDefaultDelay, this);
        }
    }

    // Rollback the fisheye preview options. Makes no changes if params were not modified. --gdm
    QnResourceWidget* centralWidget = display()->widget(Qn::CentralRole);
    if (!m_camera || !centralWidget || centralWidget->resource() != m_camera)
        return;

    if (QnMediaResourceWidget* mediaWidget = dynamic_cast<QnMediaResourceWidget*>(centralWidget))
        mediaWidget->setDewarpingParams(mediaWidget->resource()->getDewarpingParams());
}

void QnSingleCameraSettingsWidget::updateMotionWidgetFromResource()
{
    if (!m_motionWidget)
        return;
    if (m_camera && m_camera->isDtsBased())
        return;

    disconnectFromMotionWidget();

    m_motionWidget->setCamera(m_camera);
    if (!m_camera)
    {
        m_motionWidget->setVisible(false);
    }
    else
    {
        m_motionWidget->setVisible(m_cameraSupportsMotion);
    }

    updateMotionWidgetNeedControlMaxRect();

    if (auto button = m_sensitivityButtons->button(m_motionWidget->motionSensitivity()))
        button->setChecked(true);

    connectToMotionWidget();
}

void QnSingleCameraSettingsWidget::submitMotionWidgetToResource()
{
    if (!m_motionWidget || !m_camera)
        return;

    m_camera->setMotionRegionList(m_motionWidget->motionRegionList());
}

bool QnSingleCameraSettingsWidget::isReadOnly() const
{
    return m_readOnly;
}

void QnSingleCameraSettingsWidget::setReadOnly(bool readOnly)
{
    if (m_readOnly == readOnly)
        return;

    using ::setReadOnly;

    setReadOnly(ui->nameEdit, readOnly);
    setReadOnly(ui->enableAudioCheckBox, readOnly);
    setReadOnly(ui->loginEdit, readOnly);
    setReadOnly(ui->passwordEdit, readOnly);
    setReadOnly(ui->cameraScheduleWidget, readOnly);
    setReadOnly(ui->resetMotionRegionsButton, readOnly);

    for (auto button : m_sensitivityButtons->buttons())
        setReadOnly(button, readOnly);

    setReadOnly(ui->motionDetectionCheckBox, readOnly);

    if (m_motionWidget)
        setReadOnly(m_motionWidget, readOnly);

    setReadOnly(ui->imageControlWidget, readOnly);
    setReadOnly(ui->advancedSettingsWidget, readOnly);
    setReadOnly(ui->ioPortSettingsWidget, readOnly);

    if (readOnly)
        setTabEnabledSafe(Qn::ExpertCameraSettingsTab, false);

    // TODO: #vkutin #GDM Read-only for Fisheye tab

    m_readOnly = readOnly;
}

void QnSingleCameraSettingsWidget::setHasDbChanges(bool hasChanges)
{
    if (m_hasDbChanges == hasChanges)
        return;

    m_hasDbChanges = hasChanges;

    emit hasChangesChanged();
}

void QnSingleCameraSettingsWidget::disconnectFromMotionWidget()
{
    NX_ASSERT(m_motionWidget);

    disconnect(m_motionWidget, NULL, this, NULL);
}

void QnSingleCameraSettingsWidget::connectToMotionWidget()
{
    NX_ASSERT(m_motionWidget);

    connect(m_motionWidget, &QnCameraMotionMaskWidget::motionRegionListChanged, this,
        &QnSingleCameraSettingsWidget::at_dbDataChanged, Qt::UniqueConnection);

    connect(m_motionWidget, &QnCameraMotionMaskWidget::motionRegionListChanged, this,
        &QnSingleCameraSettingsWidget::at_motionRegionListChanged, Qt::UniqueConnection);
}

void QnSingleCameraSettingsWidget::showMaxFpsWarningIfNeeded()
{
    if (!m_camera)
        return;

    int maxFps = -1;
    int maxDualStreamFps = -1;

    for (const QnScheduleTask& scheduleTask : m_camera->getScheduleTasks())
    {
        switch (scheduleTask.getRecordingType())
        {
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

    QPair<int, int> fpsLimits = Qn::calculateMaxFps(m_camera);
    int maxValidFps = fpsLimits.first;
    int maxDualStreamingValidFps = fpsLimits.second;

    static const auto text = tr("FPS too high");
    if (maxValidFps < maxFps)
    {
        const auto extras = tr("FPS in the schedule was lowered from %1 to %2,"
            " which is the maximum for this camera.").arg(maxFps).arg(maxValidFps);
        QnMessageBox::warning(this, text, extras);
        hasChanges = true;
    }

    if (maxDualStreamingValidFps < maxDualStreamFps)
    {
        const auto extras =
            tr("For software motion detection, 2 FPS are reserved for the secondary stream.")
            + L'\n' + tr("FPS in the schedule was lowered from %1 to %2.")
                .arg(maxDualStreamFps).arg(maxDualStreamingValidFps);

        QnMessageBox::warning(this, text, extras);
        hasChanges = true;
    }

    if (hasChanges)
        at_dbDataChanged();
}

void QnSingleCameraSettingsWidget::updateMotionWidgetNeedControlMaxRect()
{
    if (!m_motionWidget)
        return;

    m_motionWidget->setControlMaxRects(m_camera
        && m_camera->getDefaultMotionType() == Qn::MT_HardwareGrid);
}

void QnSingleCameraSettingsWidget::updateMotionCapabilities()
{
    m_cameraSupportsMotion = m_camera ? m_camera->hasMotion() : false;
    ui->motionSensitivityGroupBox->setEnabled(m_cameraSupportsMotion);
    ui->motionControlsWidget->setVisible(m_cameraSupportsMotion);
    ui->motionAvailableLabel->setVisible(!m_cameraSupportsMotion);
}

void QnSingleCameraSettingsWidget::updateMotionAlert()
{
    m_motionAlert = QString();

    if (m_camera && m_cameraSupportsMotion
        && ui->motionDetectionCheckBox->isChecked()
        && !ui->cameraScheduleWidget->isScheduleEnabled())
    {
        m_motionAlert =
            tr("Motion detection will work only when camera is being viewed. Enable recording to make it work all the time.");
    }

    updateAlertBar();
}

void QnSingleCameraSettingsWidget::updateMotionWidgetSensitivity()
{
    if (m_motionWidget)
        m_motionWidget->setMotionSensitivity(m_sensitivityButtons->checkedId());

    m_hasMotionControlsChanges = true;
}

bool QnSingleCameraSettingsWidget::isValidMotionRegion()
{
    if (!m_motionWidget)
        return true;
    return m_motionWidget->isValidMotionRegion();
}

void QnSingleCameraSettingsWidget::updateAlertBar()
{
    switch (currentTab())
    {
        case Qn::RecordingSettingsTab:
            ui->alertBar->setText(m_recordingAlert);
            break;

        case Qn::MotionSettingsTab:
            ui->alertBar->setText(m_motionAlert);
            break;

        default:
            ui->alertBar->setText(QString());
    }
}

bool QnSingleCameraSettingsWidget::isValidSecondStream()
{
/* Do not check validness if there is no recording anyway. */
    if (!ui->cameraScheduleWidget->isScheduleEnabled())
        return true;

    if (!m_camera->hasDualStreaming())
        return true;

    auto filteredTasks = ui->cameraScheduleWidget->scheduleTasks();
    bool usesSecondStream = false;
    for (auto& task : filteredTasks)
    {
        if (task.getRecordingType() == Qn::RT_MotionAndLowQuality)
        {
            usesSecondStream = true;
            task.setRecordingType(Qn::RT_Always);
        }
    }

    /* There are no Motion+LQ tasks. */
    if (!usesSecondStream)
        return true;

    if (ui->expertSettingsWidget->isSecondStreamEnabled())
        return true;

    QnMessageBox dialog(QnMessageBoxIcon::Warning,
        tr("Secondary stream disabled for this camera"),
        tr("\"Motion + Low - Res\" recording option cannot be set."),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton);

    const auto recordAlways = dialog.addButton(
        tr("Set Recording to \"Always\""), QDialogButtonBox::YesRole);
    dialog.addButton(
        tr("Enable Secondary Stream"), QDialogButtonBox::NoRole);

    dialog.setButtonAutoDetection(QnButtonDetection::NoDetection);
    if (dialog.exec() == QDialogButtonBox::Cancel)
        return  false;

    if (dialog.clickedButton() == recordAlways)
    {
        ui->cameraScheduleWidget->setScheduleTasks(filteredTasks);
        return true;
    }

    ui->expertSettingsWidget->setSecondStreamEnabled();
    return true;
}

void QnSingleCameraSettingsWidget::setExportScheduleButtonEnabled(bool enabled)
{
    ui->cameraScheduleWidget->setExportScheduleButtonEnabled(enabled);
}


int QnSingleCameraSettingsWidget::tabIndex(Qn::CameraSettingsTab tab) const
{
    switch (tab)
    {
        case Qn::GeneralSettingsTab:
            return ui->tabWidget->indexOf(ui->generalTab);
        case Qn::RecordingSettingsTab:
            return ui->tabWidget->indexOf(ui->recordingTab);
        case Qn::IOPortsSettingsTab:
            return ui->tabWidget->indexOf(ui->ioSettingsTab);
        case Qn::MotionSettingsTab:
            return ui->tabWidget->indexOf(ui->motionTab);
        case Qn::FisheyeCameraSettingsTab:
            return ui->tabWidget->indexOf(ui->fisheyeTab);
        case Qn::AdvancedCameraSettingsTab:
            return ui->tabWidget->indexOf(ui->advancedTab);
        case Qn::ExpertCameraSettingsTab:
            return ui->tabWidget->indexOf(ui->expertTab);
        default:
            break;
    }

    /* Passing here is totally normal because we want to save the current tab while switching between dialog modes. */
    return ui->tabWidget->indexOf(ui->generalTab);
}

void QnSingleCameraSettingsWidget::setTabEnabledSafe(Qn::CameraSettingsTab tab, bool enabled)
{
    if (!enabled && currentTab() == tab)
        setCurrentTab(Qn::GeneralSettingsTab);
    ui->tabWidget->setTabEnabled(tabIndex(tab), enabled);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnSingleCameraSettingsWidget::hideEvent(QHideEvent *event)
{
    base_type::hideEvent(event);

    if (m_motionWidget)
    {
        disconnectFromMotionWidget();
        m_motionWidget->setCamera(QnResourcePtr());
    }
}

void QnSingleCameraSettingsWidget::showEvent(QShowEvent *event)
{
    base_type::showEvent(event);

    if (m_motionWidget)
    {
        updateMotionWidgetFromResource();
        connectToMotionWidget();
    }

    at_tabWidget_currentChanged(); /* Re-create motion widget if needed. */

    updateFromResource();
}

void QnSingleCameraSettingsWidget::at_motionTypeChanged()
{
    if (m_updating)
        return;

    at_dbDataChanged();
    updateMotionWidgetNeedControlMaxRect();
    ui->cameraScheduleWidget->overrideMotionType(selectedMotionType());
}

void QnSingleCameraSettingsWidget::updateIpAddressText()
{
    if (m_camera)
        ui->ipAddressEdit->setText(QnResourceDisplayInfo(m_camera).host());
    else
        ui->ipAddressEdit->clear();
}

void QnSingleCameraSettingsWidget::updateWebPageText()
{
    if (m_camera)
    {
        QString webPageAddress = QString(QLatin1String("http://%1")).arg(m_camera->getHostAddress());

        QUrl url = QUrl::fromUserInput(m_camera->getUrl());
        if (url.isValid())
        {
            QUrlQuery query(url);
            int port = query.queryItemValue(lit("http_port")).toInt();
            if (port == 0)
                port = url.port(80);

            if (port != 80 && port > 0)
                webPageAddress += QLatin1Char(':') + QString::number(url.port());
        }

        ui->webPageLink->setText(lit("<a href=\"%1\">%2</a>").arg(webPageAddress).arg(webPageAddress));
    }
    else
    {
        ui->webPageLink->clear();
    }
}

void QnSingleCameraSettingsWidget::at_resetMotionRegionsButton_clicked()
{
    QnMessageBox dialog(QnMessageBoxIcon::Question,
        tr("Reset motion regions to default?"),
        tr("This action cannot be undone."),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
        this);

    dialog.addCustomButton(QnMessageBoxCustomButton::Reset,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
    if (dialog.exec() == QDialogButtonBox::Cancel)
        return;

    if (m_motionWidget)
        m_motionWidget->clearMotion();
    m_hasMotionControlsChanges = false;
}

void QnSingleCameraSettingsWidget::at_motionRegionListChanged()
{
    m_hasMotionControlsChanges = false;
}

void QnSingleCameraSettingsWidget::at_linkActivated(const QString &urlString)
{
    QUrl url(urlString);

    bool removeCredentials = false;
    if (m_camera)
    {
        removeCredentials = 
            !m_camera->getProperty(kRemoveCredentialsFromWebPageUrl).isEmpty();
    }

    if (!m_readOnly && !removeCredentials)
    {
        url.setUserName(ui->loginEdit->text());
        url.setPassword(ui->passwordEdit->text());
    }
    QDesktopServices::openUrl(url);
}

void QnSingleCameraSettingsWidget::at_tabWidget_currentChanged()
{
    switch (currentTab())
    {
        case Qn::MotionSettingsTab:
        {
            bool hasMotionWidget = m_motionWidget != NULL;

            if (!hasMotionWidget)
                m_motionWidget = new QnCameraMotionMaskWidget(this);

            updateMotionWidgetFromResource();

            if (hasMotionWidget)
                break;

            using ::setReadOnly;
            setReadOnly(m_motionWidget, m_readOnly);
            //m_motionWidget->setReadOnly(m_readOnly);

            m_motionLayout->addWidget(m_motionWidget);

            connectToMotionWidget();

            const auto& buttons = m_sensitivityButtons->buttons();
            for (int i = 0; i < buttons.size(); ++i)
            {
                static_cast<QnSelectableButton*>(buttons[i])->setCustomPaintFunction(
                    [this, i](QPainter* painter, const QStyleOption* option, const QWidget* widget) -> bool
                    {
                        auto colors = m_motionWidget->motionSensitivityColors();
                        QColor color = i < colors.size() ? colors[i] : palette().color(QPalette::WindowText);

                        if (option->state.testFlag(QStyle::State_MouseOver))
                            color = color.lighter(120);

                        QnScopedPainterPenRollback penRollback(painter, color);
                        QnScopedPainterBrushRollback brushRollback(painter, Qt::NoBrush);
                        if (i)
                        {
                            static const qreal kButtonBackgroundOpacity = 0.3;
                            QColor brushColor(color);
                            brushColor.setAlphaF(kButtonBackgroundOpacity);
                            painter->setBrush(brushColor);
                        }

                        painter->drawRect(Geometry::eroded(QRectF(option->rect), 0.5));

                        if (auto button = qobject_cast<const QAbstractButton*>(widget))
                            painter->drawText(option->rect, Qt::AlignCenter, button->text());

                        return true;
                    });
            }

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

    updateAlertBar();
}

void QnSingleCameraSettingsWidget::at_dbDataChanged()
{
    if (m_updating)
        return;

    setHasDbChanges(true);
}

void QnSingleCameraSettingsWidget::at_cameraScheduleWidget_scheduleEnabledChanged()
{
    if (m_updating)
        return;

    ui->licensingWidget->setState(ui->cameraScheduleWidget->isScheduleEnabled()
        ? Qt::Checked
        : Qt::Unchecked);
    updateMotionAlert();
}

void QnSingleCameraSettingsWidget::at_fisheyeSettingsChanged()
{
    if (m_updating)
        return;

    at_dbDataChanged();

    // Preview the changes on the central widget

    QnResourceWidget* centralWidget = display()->widget(Qn::CentralRole);
    if (!m_camera || !centralWidget || centralWidget->resource() != m_camera)
        return;

    if (QnMediaResourceWidget* mediaWidget = dynamic_cast<QnMediaResourceWidget*>(centralWidget))
    {
        QnMediaDewarpingParams dewarpingParams = mediaWidget->dewarpingParams();
        ui->fisheyeSettingsWidget->submitToParams(dewarpingParams);
        mediaWidget->setDewarpingParams(dewarpingParams);

        QnWorkbenchItem *item = mediaWidget->item();
        QnItemDewarpingParams itemParams = item->dewarpingParams();
        itemParams.enabled = dewarpingParams.enabled;
        item->setDewarpingParams(itemParams);
    }
}
