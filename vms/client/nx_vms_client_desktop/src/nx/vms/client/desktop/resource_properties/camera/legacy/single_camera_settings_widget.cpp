#include "single_camera_settings_widget.h"
#include "ui_single_camera_settings_widget.h"
#include "legacy_camera_schedule_widget.h"
#include "legacy_camera_motion_mask_widget.h"

#include <QtCore/QBuffer>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QProcess>
#include <QtCore/QScopedValueRollback>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QButtonGroup>

#include <camera/fps_calculator.h>
#include <client/client_settings.h>
#include <core/resource/resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/client_camera.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/misc/schedule_task.h>
#include <ui/common/read_only.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/custom_style.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/delayed.h>
#include <utils/common/html.h>
#include <utils/license_usage_helper.h>

#include <nx/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/widgets/selectable_button.h>
#include <nx/vms/client/desktop/image_providers/camera_thumbnail_manager.h>

using nx::vms::client::core::Geometry;

namespace {

static const QSize kFisheyeThumbnailSize(0, 0); //unlimited size for better calibration
static const QSize kSensitivityButtonSize(34, 34);
static const QString kRemoveCredentialsFromWebPageUrl = lit("removeCredentialsFromWebPageUrl");

} // namespace

namespace nx::vms::client::desktop {

using namespace ui;

SingleCameraSettingsWidget::SingleCameraSettingsWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::SingleCameraSettingsWidget),
    m_cameraThumbnailManager(new CameraThumbnailManager()),
    m_sensitivityButtons(new QButtonGroup(this))
{
    ui->setupUi(this);
    ui->motionDetectionCheckBox->setProperty(style::Properties::kCheckBoxAsButton, true);
    ui->motionDetectionCheckBox->setForegroundRole(QPalette::ButtonText);

    m_cameraThumbnailManager->setAutoRotate(true);
    m_cameraThumbnailManager->setThumbnailSize(kFisheyeThumbnailSize);
    m_cameraThumbnailManager->setAutoRefresh(false);

    for (int i = 0; i < QnMotionRegion::kSensitivityLevelCount; ++i)
    {
        auto button = new SelectableButton(ui->motionSensitivityGroupBox);
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
        this, &SingleCameraSettingsWidget::at_tabWidget_currentChanged);
    at_tabWidget_currentChanged();

    connect(ui->nameEdit, &QLineEdit::textChanged,
        this, &SingleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->enableAudioCheckBox, &QCheckBox::stateChanged,
        this, &SingleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->loginEdit, &QLineEdit::textChanged,
        this, &SingleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->passwordEdit, &QLineEdit::textChanged,
        this, &SingleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->cameraScheduleWidget, &QnAbstractPreferencesWidget::hasChangesChanged, this,
        &SingleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->cameraScheduleWidget, &LegacyCameraScheduleWidget::scheduleEnabledChanged,
        this, &SingleCameraSettingsWidget::at_cameraScheduleWidget_scheduleEnabledChanged);
    connect(ui->cameraScheduleWidget, &LegacyCameraScheduleWidget::scheduleEnabledChanged,
        this, &SingleCameraSettingsWidget::at_dbDataChanged);
    connect(ui->cameraScheduleWidget, &LegacyCameraScheduleWidget::alert, this,
        [this](const QString& text) { m_recordingAlert = text; updateAlertBar(); });

    connect(ui->webPageLink, &QLabel::linkActivated,
        this, &SingleCameraSettingsWidget::at_linkActivated);

    connect(ui->motionDetectionCheckBox, &QCheckBox::toggled, this,
        [this]
        {
            updateMotionAlert();
            at_motionTypeChanged();
        });

    connect(m_sensitivityButtons, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked),
        this, &SingleCameraSettingsWidget::updateMotionWidgetSensitivity);

    connect(ui->resetMotionRegionsButton, &QPushButton::clicked,
        this, &SingleCameraSettingsWidget::at_resetMotionRegionsButton_clicked);

    connect(ui->pingButton, &QPushButton::clicked, this,
        [this]
        {
            /* We must always ping the same address that is displayed in the visible field. */
            auto host = ui->ipAddressEdit->text();
            menu()->trigger(action::PingAction, {Qn::TextRole, host});
        });

    connect(ui->licensingWidget, &QnLicensesProposeWidget::changed,
        this, &SingleCameraSettingsWidget::at_dbDataChanged);
    connect(ui->licensingWidget, &QnLicensesProposeWidget::changed, this,
        [this]
        {
            if (m_updating)
                return;

            ui->cameraScheduleWidget->setScheduleEnabled(ui->licensingWidget->state() == Qt::Checked);
        });

    connect(ui->expertSettingsWidget, &LegacyExpertSettingsWidget::dataChanged,
        this, &SingleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->fisheyeSettingsWidget, &FisheyeSettingsWidget::dataChanged,
        this, &SingleCameraSettingsWidget::at_fisheyeSettingsChanged);

    connect(ui->imageControlWidget, &LegacyImageControlWidget::changed,
        this, &SingleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->ioPortSettingsWidget, &LegacyIoPortSettingsWidget::dataChanged,
        this, &SingleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->advancedSettingsWidget, &LegacyAdvancedSettingsWidget::hasChangesChanged,
        this, &SingleCameraSettingsWidget::hasChangesChanged);

    connect(ui->wearableProgressWidget, &QnWearableProgressWidget::activeChanged,
        this, &SingleCameraSettingsWidget::updateWearableProgressVisibility);

    connect(ui->wearableArchiveLengthWidget, &QnArchiveLengthWidget::changed,
        this, &SingleCameraSettingsWidget::at_dbDataChanged);

    connect(ui->wearableMotionWidget, &QnWearableMotionWidget::changed,
        this, &SingleCameraSettingsWidget::at_dbDataChanged);

    updateFromResource(true);
    retranslateUi();

    Aligner* leftAligner = new Aligner(this);
    leftAligner->addWidgets({
        ui->nameLabel,
        ui->modelLabel,
        ui->firmwareLabel,
        ui->vendorLabel,
        ui->ipAddressLabel,
        ui->webPageLabel,
        ui->macAddressLabel,
        ui->loginLabel,
        ui->passwordLabel });
    leftAligner->addAligner(ui->wearableArchiveLengthWidget->aligner());

    Aligner* rightAligner = new Aligner(this);
    rightAligner->addAligner(ui->imageControlWidget->aligner());
    rightAligner->addAligner(ui->wearableMotionWidget->aligner());
}

SingleCameraSettingsWidget::~SingleCameraSettingsWidget()
{
}

void SingleCameraSettingsWidget::retranslateUi()
{
    ui->wearableInfoLabel->setText(tr("Uploaded archive can be deleted automatically, "
        "if there is no free space on a server storage. "
        "The oldest footage among all cameras on the server will be deleted first."));

    setWindowTitle(QnDeviceDependentStrings::getNameFromSet(
        resourcePool(),
        QnCameraDeviceStringSet(
            tr("Device Settings"),
            tr("Camera Settings"),
            tr("I/O Module Settings")
        ), m_camera
    ));
}

const QnVirtualCameraResourcePtr &SingleCameraSettingsWidget::camera() const
{
    return m_camera;
}

void SingleCameraSettingsWidget::setCamera(const QnVirtualCameraResourcePtr &camera)
{
    if (m_camera == camera)
        return;

    if (m_camera)
        m_camera->disconnect(this);

    m_camera = camera;
    QnVirtualCameraResourceList cameras;
    if (m_camera)
        cameras << m_camera;
    ui->advancedSettingsWidget->setCamera(camera);
    ui->cameraScheduleWidget->setCameras(cameras);
    ui->licensingWidget->setCameras(cameras);
    ui->wearableUploadWidget->setCamera(camera);
    ui->wearableProgressWidget->setCamera(camera);
    ui->wearableArchiveLengthWidget->updateFromResources(cameras);
    ui->wearableMotionWidget->updateFromResource(camera);

    if (m_camera)
    {
        connect(m_camera, &QnResource::urlChanged,      this, &SingleCameraSettingsWidget::updateIpAddressText);
        connect(m_camera, &QnResource::resourceChanged, this, &SingleCameraSettingsWidget::updateIpAddressText);
        connect(m_camera, &QnResource::urlChanged,      this, &SingleCameraSettingsWidget::updateWebPageText); // TODO: #GDM also listen to hostAddress changes?
        connect(m_camera, &QnResource::resourceChanged, this, &SingleCameraSettingsWidget::updateWebPageText); // TODO: #GDM why?
        connect(m_camera, &QnResource::resourceChanged, this, &SingleCameraSettingsWidget::updateMotionCapabilities);
    }

    updateFromResource(!isVisible());
    if (currentTab() == CameraSettingsTab::advanced)
        ui->advancedSettingsWidget->reloadData();
    retranslateUi();
}

CameraSettingsTab SingleCameraSettingsWidget::currentTab() const
{
/* Using field names here so that changes in UI file will lead to compilation errors. */

    QWidget *tab = ui->tabWidget->currentWidget();

    if (tab == ui->generalTab)
        return CameraSettingsTab::general;

    if (tab == ui->recordingTab)
        return CameraSettingsTab::recording;

    if (tab == ui->ioSettingsTab)
        return CameraSettingsTab::io;

    if (tab == ui->motionTab)
        return CameraSettingsTab::motion;

    if (tab == ui->advancedTab)
        return CameraSettingsTab::advanced;

    if (tab == ui->expertTab)
        return CameraSettingsTab::expert;

    if (tab == ui->fisheyeTab)
        return CameraSettingsTab::fisheye;

    NX_ASSERT(false, "Should never get here");
    return CameraSettingsTab::general;
}

void SingleCameraSettingsWidget::setCurrentTab(CameraSettingsTab tab)
{
/* Using field names here so that changes in UI file will lead to compilation errors. */

    if (!ui->tabWidget->isTabEnabled(tabIndex(tab)))
    {
        ui->tabWidget->setCurrentWidget(ui->generalTab);
        return;
    }

    switch (tab)
    {
        case CameraSettingsTab::general:
            ui->tabWidget->setCurrentWidget(ui->generalTab);
            break;
        case CameraSettingsTab::recording:
            ui->tabWidget->setCurrentWidget(ui->recordingTab);
            break;
        case CameraSettingsTab::motion:
            ui->tabWidget->setCurrentWidget(ui->motionTab);
            break;
        case CameraSettingsTab::fisheye:
            ui->tabWidget->setCurrentWidget(ui->fisheyeTab);
            break;
        case CameraSettingsTab::advanced:
            ui->tabWidget->setCurrentWidget(ui->advancedTab);
            break;
        case CameraSettingsTab::io:
            ui->tabWidget->setCurrentWidget(ui->ioSettingsTab);
            break;
        case CameraSettingsTab::expert:
            ui->tabWidget->setCurrentWidget(ui->expertTab);
            break;
        default:
            qnWarning("Invalid camera settings tab '%1'.", static_cast<int>(tab));
            break;
    }
}

bool SingleCameraSettingsWidget::hasDbChanges() const
{
    return m_hasDbChanges;
}

bool SingleCameraSettingsWidget::hasAdvancedCameraChanges() const
{
    return ui->advancedSettingsWidget->hasChanges();
}

bool SingleCameraSettingsWidget::hasChanges() const
{
    return hasDbChanges() || hasAdvancedCameraChanges();
}

void SingleCameraSettingsWidget::setLockedMode(bool locked)
{
    if (m_lockedMode == locked)
        return;

    m_lockedMode = locked;
    updateFromResource();
}

Qn::MotionType SingleCameraSettingsWidget::selectedMotionType() const
{
    if (!m_camera)
        return Qn::MotionType::MT_Default;

    if (!ui->motionDetectionCheckBox->isChecked())
        return Qn::MotionType::MT_NoMotion;

    return m_camera->getDefaultMotionType();
}

void SingleCameraSettingsWidget::submitToResource()
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
        {
            if ((m_camera->isMultiSensorCamera() || m_camera->isNvr())
                && !m_camera->getGroupId().isEmpty())
            {
                QnClientCameraResource::setAuthToCameraGroup(m_camera, loginEditAuth);
            }
            else
            {
                m_camera->setAuth(loginEditAuth);
            }
        }

        ui->cameraScheduleWidget->applyChanges();

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

        if (m_camera->hasFlags(Qn::wearable_camera))
        {
            ui->wearableMotionWidget->submitToResource(m_camera);
            ui->wearableArchiveLengthWidget->submitToResources({m_camera});
        }

        setHasDbChanges(false);
    }

    if (hasAdvancedCameraChanges())
        ui->advancedSettingsWidget->submitToResource();
}

void SingleCameraSettingsWidget::updateFromResource(bool silent)
{
    setTabEnabledSafe(CameraSettingsTab::general, !m_lockedMode);

    QScopedValueRollback<bool> updateRollback(m_updating, true);

    QnVirtualCameraResourceList cameras;
    if (m_camera)
        cameras << m_camera;

    ui->imageControlWidget->updateFromResources(cameras);

    updateWearableProgressVisibility();

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
        ui->wearableUploadWidget->setVisible(false);
        ui->wearableMotionWidget->setVisible(false);
    }
    else
    {
        bool isWearable = m_camera->hasFlags(Qn::wearable_camera);
        bool hasVideo = m_camera->hasVideo(0);
        bool hasAudio = m_camera->isAudioSupported();

        ui->wearableUploadWidget->setVisible(isWearable);
        ui->wearableMotionWidget->setVisible(isWearable);
        ui->modelEdit->setVisible(!isWearable);
        ui->modelLabel->setVisible(!isWearable);
        ui->firmwareEdit->setVisible(!isWearable);
        ui->firmwareLabel->setVisible(!isWearable);
        ui->vendorEdit->setVisible(!isWearable);
        ui->vendorLabel->setVisible(!isWearable);
        ui->addressGroupBox->setVisible(!isWearable);
        ui->authenticationGroupBox->setVisible(!isWearable);
        ui->topFormLayout->setVerticalSpacing(isWearable ? 2 : 8);

        if (isWearable)
        {
            ui->addressOrArchiveLengthWidget->setCurrentWidget(ui->wearableArchiveLengthPage);
            ui->authenticationOrWearableInfoWidget->setCurrentWidget(ui->wearableInfoPage);
        }
        else
        {
            ui->addressOrArchiveLengthWidget->setCurrentWidget(ui->addressPage);
            ui->authenticationOrWearableInfoWidget->setCurrentWidget(ui->authenticationPage);
        }

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

        setTabEnabledSafe(CameraSettingsTab::advanced, !m_lockedMode && !isWearable);
        setTabEnabledSafe(CameraSettingsTab::recording, !dtsBased && (hasAudio || hasVideo) && !m_lockedMode && !isWearable);
        setTabEnabledSafe(CameraSettingsTab::motion, !dtsBased && hasVideo && !m_lockedMode && !isWearable);
        setTabEnabledSafe(CameraSettingsTab::expert, !m_lockedMode);
        setTabEnabledSafe(CameraSettingsTab::io, isIoModule && !m_lockedMode && !isWearable);
        setTabEnabledSafe(CameraSettingsTab::fisheye, !isIoModule && !m_lockedMode && !isWearable);


        if (!dtsBased)
        {
            auto supported = m_camera->supportedMotionType();
            auto motionType = m_camera->getMotionType();
            auto mdEnabled = supported.testFlag(motionType) && motionType != Qn::MotionType::MT_NoMotion;
            ui->motionDetectionCheckBox->setChecked(mdEnabled);
            ui->cameraScheduleWidget->setMotionDetectionAllowed(mdEnabled);

            updateMotionCapabilities();
            updateMotionWidgetFromResource();
        }
        ui->expertSettingsWidget->updateFromResources({m_camera});

        m_cameraThumbnailManager->selectCamera(m_camera);
        m_cameraThumbnailManager->refreshSelectedCamera();
        ui->fisheyeSettingsWidget->updateFromParams(m_camera->getDewarpingParams(),
            m_cameraThumbnailManager.data());
    }

    /* After overrideMotionType is set. */
    ui->cameraScheduleWidget->loadDataToUi();

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
            executeDelayedParented(callback, this);
        }
    }

    // Rollback the fisheye preview options. Makes no changes if params were not modified. --gdm
    QnResourceWidget* centralWidget = display()->widget(Qn::CentralRole);
    if (!m_camera || !centralWidget || centralWidget->resource() != m_camera)
        return;

    if (QnMediaResourceWidget* mediaWidget = dynamic_cast<QnMediaResourceWidget*>(centralWidget))
        mediaWidget->setDewarpingParams(mediaWidget->resource()->getDewarpingParams());
}

void SingleCameraSettingsWidget::updateMotionWidgetFromResource()
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

void SingleCameraSettingsWidget::submitMotionWidgetToResource()
{
    if (!m_motionWidget || !m_camera)
        return;

    m_camera->setMotionRegionList(m_motionWidget->motionRegionList());
}

bool SingleCameraSettingsWidget::isReadOnly() const
{
    return m_readOnly;
}

void SingleCameraSettingsWidget::setReadOnly(bool readOnly)
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
    setReadOnly(ui->wearableMotionWidget, readOnly);
    setReadOnly(ui->expertSettingsWidget, readOnly);

    // TODO: #vkutin #GDM Read-only for Fisheye tab

    m_readOnly = readOnly;
}

void SingleCameraSettingsWidget::setHasDbChanges(bool hasChanges)
{
    if (m_hasDbChanges == hasChanges)
        return;

    m_hasDbChanges = hasChanges;

    emit hasChangesChanged();
}

void SingleCameraSettingsWidget::disconnectFromMotionWidget()
{
    NX_ASSERT(m_motionWidget);
    m_motionWidget->disconnect(this);
}

void SingleCameraSettingsWidget::connectToMotionWidget()
{
    NX_ASSERT(m_motionWidget);

    connect(m_motionWidget, &LegacyCameraMotionMaskWidget::motionRegionListChanged, this,
        &SingleCameraSettingsWidget::at_dbDataChanged, Qt::UniqueConnection);

    connect(m_motionWidget, &LegacyCameraMotionMaskWidget::motionRegionListChanged, this,
        &SingleCameraSettingsWidget::at_motionRegionListChanged, Qt::UniqueConnection);
}

void SingleCameraSettingsWidget::showMaxFpsWarningIfNeeded()
{
    if (!m_camera)
        return;

    int maxFps = -1;
    int maxDualStreamFps = -1;

    for (const QnScheduleTask& scheduleTask: m_camera->getScheduleTasks())
    {
        switch (scheduleTask.recordingType)
        {
            case Qn::RecordingType::never:
                continue;
            case Qn::RecordingType::motionAndLow:
                maxDualStreamFps = qMax(maxDualStreamFps, scheduleTask.fps);
                break;
            case Qn::RecordingType::always:
            case Qn::RecordingType::motionOnly:
                maxFps = qMax(maxFps, scheduleTask.fps);
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

void SingleCameraSettingsWidget::updateMotionWidgetNeedControlMaxRect()
{
    if (!m_motionWidget)
        return;

    m_motionWidget->setControlMaxRects(m_camera
        && m_camera->getDefaultMotionType() == Qn::MotionType::MT_HardwareGrid);
}

void SingleCameraSettingsWidget::updateMotionCapabilities()
{
    m_cameraSupportsMotion = m_camera ? m_camera->hasMotion() : false;
    ui->motionSensitivityGroupBox->setEnabled(m_cameraSupportsMotion);
    ui->motionControlsWidget->setVisible(m_cameraSupportsMotion);
    ui->motionAvailableLabel->setVisible(!m_cameraSupportsMotion);
}

void SingleCameraSettingsWidget::updateMotionAlert()
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

void SingleCameraSettingsWidget::updateMotionWidgetSensitivity()
{
    if (m_motionWidget)
        m_motionWidget->setMotionSensitivity(m_sensitivityButtons->checkedId());

    m_hasMotionControlsChanges = true;
}

bool SingleCameraSettingsWidget::isValidMotionRegion()
{
    if (!m_motionWidget)
        return true;
    return m_motionWidget->isValidMotionRegion();
}

void SingleCameraSettingsWidget::updateAlertBar()
{
    switch (currentTab())
    {
        case CameraSettingsTab::recording:
            ui->alertBar->setText(m_recordingAlert);
            break;

        case CameraSettingsTab::motion:
            ui->alertBar->setText(m_motionAlert);
            break;

        default:
            ui->alertBar->setText(QString());
    }
}

void SingleCameraSettingsWidget::updateWearableProgressVisibility()
{
    if (!m_camera || !m_camera->hasFlags(Qn::wearable_camera)) {
        ui->wearableProgressWidget->setVisible(false);
        return;
    }

    ui->wearableProgressWidget->setVisible(ui->wearableProgressWidget->isActive());
}

bool SingleCameraSettingsWidget::isValidSecondStream()
{
/* Do not check validness if there is no recording anyway. */
    if (!ui->cameraScheduleWidget->isScheduleEnabled())
        return true;

    if (!m_camera->hasDualStreamingInternal())
        return true;

    auto filteredTasks = ui->cameraScheduleWidget->scheduleTasks();
    bool usesSecondStream = false;
    for (auto& task : filteredTasks)
    {
        if (task.recordingType == Qn::RecordingType::motionAndLow)
        {
            usesSecondStream = true;
            task.recordingType = Qn::RecordingType::always;
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
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton, this);

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

void SingleCameraSettingsWidget::setExportScheduleButtonEnabled(bool enabled)
{
    ui->cameraScheduleWidget->setExportScheduleButtonEnabled(enabled);
}


int SingleCameraSettingsWidget::tabIndex(CameraSettingsTab tab) const
{
    switch (tab)
    {
        case CameraSettingsTab::general:
            return ui->tabWidget->indexOf(ui->generalTab);
        case CameraSettingsTab::recording:
            return ui->tabWidget->indexOf(ui->recordingTab);
        case CameraSettingsTab::io:
            return ui->tabWidget->indexOf(ui->ioSettingsTab);
        case CameraSettingsTab::motion:
            return ui->tabWidget->indexOf(ui->motionTab);
        case CameraSettingsTab::fisheye:
            return ui->tabWidget->indexOf(ui->fisheyeTab);
        case CameraSettingsTab::advanced:
            return ui->tabWidget->indexOf(ui->advancedTab);
        case CameraSettingsTab::expert:
            return ui->tabWidget->indexOf(ui->expertTab);
        default:
            break;
    }

    /* Passing here is totally normal because we want to save the current tab while switching between dialog modes. */
    return ui->tabWidget->indexOf(ui->generalTab);
}

void SingleCameraSettingsWidget::setTabEnabledSafe(CameraSettingsTab tab, bool enabled)
{
    if (!enabled && currentTab() == tab)
        setCurrentTab(CameraSettingsTab::general);
    ui->tabWidget->setTabEnabled(tabIndex(tab), enabled);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void SingleCameraSettingsWidget::hideEvent(QHideEvent *event)
{
    base_type::hideEvent(event);

    if (m_motionWidget)
    {
        disconnectFromMotionWidget();
        m_motionWidget->setCamera(QnResourcePtr());
    }
}

void SingleCameraSettingsWidget::showEvent(QShowEvent *event)
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

void SingleCameraSettingsWidget::at_motionTypeChanged()
{
    if (m_updating)
        return;

    at_dbDataChanged();
    updateMotionWidgetNeedControlMaxRect();
    ui->cameraScheduleWidget->setMotionDetectionAllowed(
        m_camera && ui->motionDetectionCheckBox->isChecked());
}

void SingleCameraSettingsWidget::updateIpAddressText()
{
    if (m_camera)
        ui->ipAddressEdit->setText(QnResourceDisplayInfo(m_camera).host());
    else
        ui->ipAddressEdit->clear();
}

void SingleCameraSettingsWidget::updateWebPageText()
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

        ui->webPageLink->setText(makeHref(webPageAddress, webPageAddress));
    }
    else
    {
        ui->webPageLink->clear();
    }
}

void SingleCameraSettingsWidget::at_resetMotionRegionsButton_clicked()
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

void SingleCameraSettingsWidget::at_motionRegionListChanged()
{
    m_hasMotionControlsChanges = false;
}

void SingleCameraSettingsWidget::at_linkActivated(const QString &urlString)
{
    QUrl url(urlString);

    const bool removeCredentials = m_camera
        && !m_camera->getProperty(kRemoveCredentialsFromWebPageUrl).isEmpty();

    if (!m_readOnly && !removeCredentials)
    {
        url.setUserName(ui->loginEdit->text());
        url.setPassword(ui->passwordEdit->text());
    }
    QDesktopServices::openUrl(url);
}

void SingleCameraSettingsWidget::at_tabWidget_currentChanged()
{
    switch (currentTab())
    {
        case CameraSettingsTab::motion:
        {
            bool hasMotionWidget = m_motionWidget != NULL;

            if (!hasMotionWidget)
                m_motionWidget = new LegacyCameraMotionMaskWidget(this);

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
                static_cast<SelectableButton*>(buttons[i])->setCustomPaintFunction(
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

        case CameraSettingsTab::advanced:
        {
            ui->advancedSettingsWidget->reloadData();
            break;
        }

        case CameraSettingsTab::fisheye:
        {
            m_cameraThumbnailManager->refreshSelectedCamera();
            break;
        }

        default:
            break;
    }

    updateAlertBar();
}

void SingleCameraSettingsWidget::at_dbDataChanged()
{
    if (m_updating)
        return;

    setHasDbChanges(true);
}

void SingleCameraSettingsWidget::at_cameraScheduleWidget_scheduleEnabledChanged()
{
    if (m_updating)
        return;

    ui->licensingWidget->setState(ui->cameraScheduleWidget->isScheduleEnabled()
        ? Qt::Checked
        : Qt::Unchecked);
    updateMotionAlert();
}

void SingleCameraSettingsWidget::at_fisheyeSettingsChanged()
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
        auto itemParams = item->dewarpingParams();
        itemParams.enabled = dewarpingParams.enabled;
        item->setDewarpingParams(itemParams);
    }
}

} // namespace nx::vms::client::desktop
