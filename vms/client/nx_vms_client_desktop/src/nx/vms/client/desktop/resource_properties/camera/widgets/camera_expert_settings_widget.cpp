// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_expert_settings_widget.h"
#include "ui_camera_expert_settings_widget.h"

#include <cmath>

#include <core/ptz/ptz_constants.h>
#include <nx/network/http/http_types.h>
#include <nx/vms/api/types/motion_types.h>
#include <nx/vms/api/types/rtp_types.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/utils/check_box_utils.h>
#include <nx/vms/client/desktop/common/utils/combo_box_utils.h>
#include <nx/vms/client/desktop/common/utils/spin_box_utils.h>
#include <nx/vms/client/desktop/common/widgets/hint_button.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/utils/log/log.h>
#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include "private/motion_stream_alerts.h"
#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

namespace {

static const int kHintUserRole = Qt::UserRole + 2;

void activateLayouts(const std::initializer_list<QWidget*>& widgets)
{
    for (auto widget: widgets)
    {
        if (auto layout = widget->layout())
            layout->activate();
    }
}

static const int kDefaultRtspPort = 554;

static constexpr qreal kSensitivitySliderResolution = 1000.0;

qreal sliderValueToPtzSensitivity(int value)
{
    return std::clamp(
        pow(10.0, qreal(value) / kSensitivitySliderResolution),
        Ptz::kMinimumSensitivity,
        Ptz::kMaximumSensitivity);
}

int ptzSensitivityToSliderValue(qreal sensitivity)
{
    return int(std::round(log10(sensitivity) * kSensitivitySliderResolution));
}

bool isSystemPtzPreset(const CameraSettingsDialogState& state)
{
    return state.expert.preferredPtzPresetType.equals(nx::core::ptz::PresetType::system);
}

bool isNativePtzPreset(const CameraSettingsDialogState& state)
{
    return state.expert.preferredPtzPresetType.equals(nx::core::ptz::PresetType::native);
}

bool isAutoPtzPreset(const CameraSettingsDialogState& state)
{
    return state.expert.preferredPtzPresetType.equals(nx::core::ptz::PresetType::undefined);
}

void fillProfiles(
    const CameraSettingsDialogState& state,
    const UserEditableMultiple<QString>& userDefinedProfile,
    ComboBoxWithHint* comboBox,
    bool isPrimary)
{
    static const QString kDefaultProfileHint = CameraExpertSettingsWidget::tr("default");

    comboBox->clear();
    combo_box_utils::insertMultipleValuesItem(comboBox);
    comboBox->addItem(CameraExpertSettingsWidget::tr("Auto", "Automatic profile selection"), QString());
    for (const auto& p : state.expert.availableProfiles)
    {
        using namespace nx::vms::api;
        comboBox->addItem(QString::fromStdString(p.name), QString::fromStdString(p.token));
        if (p.state == DeviceProfile::State::primaryDefault && isPrimary
            || p.state == DeviceProfile::State::secondaryDefault && !isPrimary)
        {
            comboBox->setItemHint(comboBox->count() - 1, kDefaultProfileHint);
        }
    }

    if (userDefinedProfile.hasValue())
    {
        const QVariant profileToken = QVariant::fromValue(userDefinedProfile());
        auto index = comboBox->findData(profileToken);
        if (index == -1)
            index = 0; //< Use 'auto' value in case of selected profile is unavailable now.
        comboBox->setCurrentIndex(index);
    }
    else
    {
        comboBox->setCurrentIndex(/*multiple values*/ 0);
    }
}

} // namespace

CameraExpertSettingsWidget::CameraExpertSettingsWidget(
    CameraSettingsDialogStore* store,
    QWidget* parent)
    :
    QWidget(parent),
    ui(new Ui::CameraExpertSettingsWidget)
{
    ui->setupUi(this);

    NX_ASSERT(parent);
    auto scrollBar = new SnappedScrollBar(ui->contentContainer);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());
    scrollBar->setUseMaximumSpace(true);

    check_box_utils::autoClearTristate(ui->secondStreamDisableCheckBox);
    check_box_utils::autoClearTristate(ui->forcedPanTiltCheckBox);
    check_box_utils::autoClearTristate(ui->forcedZoomCheckBox);
    check_box_utils::autoClearTristate(ui->trustCameraTimeCheckBox);
    check_box_utils::autoClearTristate(ui->differentPtzSensitivitiesCheckBox);
    check_box_utils::autoClearTristate(ui->remoteArchiveMotionDetectionCheckBox);

    spin_box_utils::autoClearSpecialValue(ui->panSensitivitySpinBox, 1.0);

    setWarningStyle(ui->bitrateIncreaseWarningLabel);
    setWarningStyle(ui->generalWarningLabel);
    setWarningStyle(ui->logicalIdWarningLabel);
    setWarningStyle(ui->presetTypeLimitationsLabel);

    ui->bitrateIncreaseWarningLabel->setVisible(false);

    ui->useMedia2ToFetchProfilesComboBox->clear();
    combo_box_utils::insertMultipleValuesItem(ui->useMedia2ToFetchProfilesComboBox);
    ui->useMedia2ToFetchProfilesComboBox->addItem(tr("Auto",
        "Let server determine if Media2 should be used to fetch profiles"),
        QVariant::fromValue(nx::core::resource::UsingOnvifMedia2Type::autoSelect));
    ui->useMedia2ToFetchProfilesComboBox->addItem(tr("Use if supported"),
        QVariant::fromValue(nx::core::resource::UsingOnvifMedia2Type::useIfSupported));
    ui->useMedia2ToFetchProfilesComboBox->addItem(tr("Never"),
        QVariant::fromValue(nx::core::resource::UsingOnvifMedia2Type::neverUse));

    ui->comboBoxTransport->clear();
    combo_box_utils::insertMultipleValuesItem(ui->comboBoxTransport);
    ui->comboBoxTransport->addItem(tr("Auto", "Automatic RTP transport type"),
        QVariant::fromValue(vms::api::RtpTransportType::automatic));
    ui->comboBoxTransport->addItem("TCP", QVariant::fromValue(vms::api::RtpTransportType::tcp));
    ui->comboBoxTransport->addItem("UDP", QVariant::fromValue(vms::api::RtpTransportType::udp));

    ui->iconLabel->setPixmap(qnSkin->pixmap("theme/warning.png"));
    ui->iconLabel->setScaledContents(true);

    setWarningFrame(ui->warningContainer);

    connect(store, &CameraSettingsDialogStore::stateChanged,
        this, &CameraExpertSettingsWidget::loadState);

    connect(ui->settingsDisableControlCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setCameraControlDisabled);

    connect(ui->keepCameraTimeSettingsCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setKeepCameraTimeSettings);

    connect(ui->secondStreamDisableCheckBox, &QCheckBox::clicked, store,
        [store](bool checked) { store->setDualStreamingDisabled(checked); });

    connect(ui->bitratePerGopCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setUseBitratePerGOP);

    connect(ui->useMedia2ToFetchProfilesComboBox, QnComboboxCurrentIndexChanged, store,
        [this, store](int index)
        {
            store->setUseMedia2ToFetchProfiles(
                ui->useMedia2ToFetchProfilesComboBox->itemData(index)
                .value<nx::core::resource::UsingOnvifMedia2Type>());
        });

    connect(ui->checkBoxPrimaryRecorder, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setPrimaryRecordingDisabled);

    connect(ui->checkBoxSecondaryRecorder, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setSecondaryRecordingDisabled);

    connect(ui->forcedPanTiltCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setForcedPtzPanTiltCapability);

    connect(ui->forcedZoomCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setForcedPtzZoomCapability);

    connect(ui->autoMediaPortCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setAutoMediaPortUsed);

    connect(ui->customMediaPortSpinBox, QnSpinboxIntValueChanged,
        store, &CameraSettingsDialogStore::setCustomMediaPort);

    connect(ui->useAutoWebPagePortCheckBox, &QCheckBox::clicked, this,
        [this, store](bool checked)
        {
            store->setCustomWebPagePort(checked ? 0 : nx::network::http::DEFAULT_HTTP_PORT);
        });

    connect(ui->customWebPagePortSpinBox, QnSpinboxIntValueChanged,
        store, &CameraSettingsDialogStore::setCustomWebPagePort);

    connect(ui->trustCameraTimeCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setTrustCameraTime);

    connect(ui->remoteArchiveAutoImportCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setRemoteArchiveAutoImportEnabled);

    connect(ui->logicalIdSpinBox, QnSpinboxIntValueChanged,
        store, &CameraSettingsDialogStore::setLogicalId);

    connect(ui->generateLogicalIdButton, &QPushButton::clicked,
        store, &CameraSettingsDialogStore::generateLogicalId);

    connect(ui->resetLogicalIdButton,
        &QPushButton::clicked,
        store,
        [store]() { store->setLogicalId({}); });

    connect(ui->prefferedPtzPresetTypeAutoRadioButton,
        &QRadioButton::toggled,
        [store](bool checked)
        {
            if (checked)
                store->setPreferredPtzPresetType(nx::core::ptz::PresetType::undefined);
        });

    connect(ui->prefferedPtzPresetTypeNativeRadioButton,
        &QRadioButton::toggled,
        [store](bool checked)
        {
            if (checked)
                store->setPreferredPtzPresetType(nx::core::ptz::PresetType::native);
        });

    connect(ui->prefferedPtzPresetTypeSystemRadioButton,
        &QRadioButton::toggled,
        [store](bool checked)
        {
            if (checked)
                store->setPreferredPtzPresetType(nx::core::ptz::PresetType::system);
        });

    connect(ui->comboBoxTransport, QnComboboxCurrentIndexChanged, store,
        [this, store](int index)
        {
            store->setRtpTransportType(
                ui->comboBoxTransport->itemData(index).value<vms::api::RtpTransportType>());
        });

    connect(ui->comboBoxPrimaryProfile, QnComboboxCurrentIndexChanged, store,
        [this, store](int index)
        {
            store->setForcedPrimaryProfile(
                ui->comboBoxPrimaryProfile->itemData(index).value<QString>());
        });

    connect(ui->comboBoxSecondaryProfile, QnComboboxCurrentIndexChanged, store,
        [this, store](int index)
        {
            store->setForcedSecondaryProfile(
                ui->comboBoxSecondaryProfile->itemData(index).value<QString>());
        });
    ui->comboBoxPrimaryProfile->setHintRole(kHintUserRole);
    ui->comboBoxSecondaryProfile->setHintRole(kHintUserRole);

    connect(ui->restoreDefaultsButton, &QPushButton::clicked,
        store, &CameraSettingsDialogStore::resetExpertSettings);

    const auto forceDetectionButton = new QPushButton(ui->motionImplicitlyDisabledAlertBar);
    forceDetectionButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    forceDetectionButton->setText(tr("Force Motion Detection"));
    ui->motionImplicitlyDisabledAlertBar->verticalLayout()->addWidget(forceDetectionButton);

    connect(forceDetectionButton, &QPushButton::clicked, store,
        [store]() { store->forceMotionDetection(); });

    static const int kSensitivitySliderMin = ptzSensitivityToSliderValue(Ptz::kMinimumSensitivity);
    static const int kSensitivitySliderMax = ptzSensitivityToSliderValue(Ptz::kMaximumSensitivity);

    spin_box_utils::autoClearSpecialValue(ui->panSensitivitySpinBox, Ptz::kDefaultSensitivity);
    ui->panSensitivitySpinBox->setRange(Ptz::kMinimumSensitivity, Ptz::kMaximumSensitivity);
    ui->panSensitivitySpinBox->setSingleStep(0.1);
    ui->panSensitivitySlider->setRange(kSensitivitySliderMin, kSensitivitySliderMax);
    ui->panSensitivitySlider->setSingleStep(0.1 * kSensitivitySliderResolution);
    ui->panSensitivitySlider->setPageStep(1.0 * kSensitivitySliderResolution);

    spin_box_utils::autoClearSpecialValue(ui->tiltSensitivitySpinBox, Ptz::kDefaultSensitivity);
    ui->tiltSensitivitySpinBox->setRange(Ptz::kMinimumSensitivity, Ptz::kMaximumSensitivity);
    ui->tiltSensitivitySpinBox->setSingleStep(0.1);
    ui->tiltSensitivitySlider->setRange(kSensitivitySliderMin, kSensitivitySliderMax);
    ui->tiltSensitivitySlider->setSingleStep(0.1 * kSensitivitySliderResolution);
    ui->tiltSensitivitySlider->setPageStep(1.0 * kSensitivitySliderResolution);

    auto aligner = new Aligner(this);
    aligner->addWidgets({ui->panSensitivityLabel, ui->tiltSensitivityLabel});
    m_aligners.push_back(aligner);

    connect(ui->differentPtzSensitivitiesCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setDifferentPtzPanTiltSensitivities);

    connect(ui->panSensitivitySpinBox, QnSpinboxDoubleValueChanged,
        store, &CameraSettingsDialogStore::setPtzPanSensitivity);

    connect(ui->panSensitivitySlider, &QSlider::valueChanged, this,
        [store](int value)
        {
            store->setPtzPanSensitivity(sliderValueToPtzSensitivity(value));
        });

    connect(ui->tiltSensitivitySpinBox, QnSpinboxDoubleValueChanged,
        store, &CameraSettingsDialogStore::setPtzTiltSensitivity);

    connect(ui->tiltSensitivitySlider, &QSlider::valueChanged, this,
        [store](int value)
        {
            store->setPtzTiltSensitivity(sliderValueToPtzSensitivity(value));
        });

    connect(ui->remoteArchiveMotionDetectionCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setRemoteArchiveMotionDetectionEnabled);

    setHelpTopic(this, Qn::CameraSettingsExpertPage_Help);
    setHelpTopic(ui->secondStreamDisableCheckBox, Qn::CameraSettings_SecondStream_Help);
    setHelpTopic(ui->settingsDisableControlCheckBox, Qn::CameraSettings_Expert_SettingsControl_Help);
    setHelpTopic(ui->keepCameraTimeSettingsCheckBox, Qn::CameraSettings_Expert_SettingsControl_Help);
    setHelpTopic(ui->checkBoxPrimaryRecorder, Qn::CameraSettings_Expert_DisableArchivePrimary_Help);
    setHelpTopic(ui->checkBoxSecondaryRecorder, Qn::CameraSettings_Expert_DisableArchivePrimary_Help);
    setHelpTopic(ui->groupBoxRTP, Qn::CameraSettings_Expert_Rtp_Help);
    setHelpTopic(ui->useMedia2ToFetchProfilesGroupBox, Qn::CameraSettings_Onvif_Help);

    ui->settingsDisableControlCheckBox->setHint(tr("Server will not change any cameras settings, "
        "it will receive and use camera stream as-is."));

    ui->keepCameraTimeSettingsCheckBox->setHint(
        tr("Server will not push time settings to the camera."));

    setHelpTopic(ui->bitratePerGopCheckBox, Qn::CameraSettings_Expert_SettingsControl_Help);
    ui->bitratePerGopCheckBox->setHint(tr("Helps fix image quality issues on some cameras; "
        "for others will cause significant bitrate increase."));

    const auto logicalIdHint = HintButton::createGroupBoxHint(ui->logicalIdGroupBox);
    logicalIdHint->setHintText(
        tr("Custom number that can be assigned to a camera for quick identification and access"));

    ui->remoteArchiveAutoImportCheckBox->setHint(tr("Video recorded on the internal "
        "camera's storage will be automatically uploaded to the main archive."));
}

CameraExpertSettingsWidget::~CameraExpertSettingsWidget()
{
}

void CameraExpertSettingsWidget::loadState(const CameraSettingsDialogState& state)
{
    if (state.devicesCount == 0)
        return;

    // Disable irrelevant controls leaving only Logical Id and Media Streaming for DTS, Virtual
    // Cameras or I/O Modules.
    const bool supportsVideoStreamControl = state.supportsVideoStreamControl();
    ui->groupBoxMotionDetection->setEnabled(supportsVideoStreamControl);
    ui->remoteArchiveAutoImportGroupBox->setEnabled(supportsVideoStreamControl);
    ui->settingsGroupBox->setEnabled(supportsVideoStreamControl);
    ui->timeSettingsGroupBox->setEnabled(supportsVideoStreamControl);
    ui->useMedia2ToFetchProfilesGroupBox->setEnabled(supportsVideoStreamControl);

    ui->groupBoxRTP->setEnabled(state.supportsSchedule() /*has video and/or audio stream(s)*/);

    const bool hasDualStreaming =
        state.devicesDescription.hasDualStreamingCapability != CombinedValue::None;

    const bool hasArecontCameras =
        state.devicesDescription.isArecontCamera != CombinedValue::None;

    ui->settingsDisableControlCheckBox->setHidden(hasArecontCameras);
    ui->bitratePerGopCheckBox->setHidden(hasArecontCameras);

    if (!hasArecontCameras)
    {
        ui->settingsDisableControlCheckBox->setVisible(state.recording.parametersAvailable);
        ui->settingsDisableControlCheckBox->setEnabled(state.settingsOptimizationEnabled);

        const bool controlDisabledForAll = state.settingsOptimizationEnabled
            && state.expert.cameraControlDisabled.valueOr(false);

        check_box_utils::setupTristateCheckbox(
            ui->settingsDisableControlCheckBox,
            state.expert.cameraControlDisabled.hasValue() || !state.settingsOptimizationEnabled,
            controlDisabledForAll);
    }

    // Hide certain fields for RTSP/HTTP links.
    const bool isNetworkLink = state.isSingleCamera() && state.singleCameraProperties.networkLink;
    ui->timeSettingsGroupBox->setHidden(!state.settingsOptimizationEnabled || isNetworkLink);
    check_box_utils::setupTristateCheckbox(ui->keepCameraTimeSettingsCheckBox,
        state.expert.keepCameraTimeSettings.hasValue() || !state.settingsOptimizationEnabled,
        !state.settingsOptimizationEnabled
            || state.expert.keepCameraTimeSettings.valueOr(true));

    check_box_utils::setupTristateCheckbox(ui->bitratePerGopCheckBox,
        state.expert.useBitratePerGOP);

    ui->useMedia2ToFetchProfilesGroupBox->setVisible(state.expert.areOnvifSettingsApplicable);

    const auto index = ui->useMedia2ToFetchProfilesComboBox->findData(
        QVariant::fromValue(state.expert.useMedia2ToFetchProfiles()));
    ui->useMedia2ToFetchProfilesComboBox->setCurrentIndex(index);

    ::setReadOnly(ui->useMedia2ToFetchProfilesComboBox, state.readOnly);
    ::setReadOnly(ui->comboBoxPrimaryProfile, state.readOnly);
    ::setReadOnly(ui->comboBoxSecondaryProfile, state.readOnly);

    // TODO: #vkutin #sivanov Should we disable it too when camera control is disabled?
    ui->bitratePerGopCheckBox->setEnabled(state.settingsOptimizationEnabled);

    ui->bitrateIncreaseWarningLabel->setVisible(ui->bitratePerGopCheckBox->isEnabled()
        && ui->bitratePerGopCheckBox->isChecked());

    ::setReadOnly(ui->settingsDisableControlCheckBox, state.readOnly);
    ::setReadOnly(ui->bitratePerGopCheckBox, state.readOnly);

    // Secondary Stream.

    ui->secondStreamDisableCheckBox->setVisible(hasDualStreaming);
    if (hasDualStreaming)
    {
        check_box_utils::setupTristateCheckbox(
            ui->secondStreamDisableCheckBox,
            state.expert.dualStreamingDisabled.hasValue(),
            state.expert.dualStreamingDisabled.valueOr(false));
    }
    ui->secondStreamDisableCheckBox->setEnabled(state.settingsOptimizationEnabled);
    ::setReadOnly(ui->secondStreamDisableCheckBox, state.readOnly);

    // Motion detection.

    const bool remoteArchiveMdSupported =
        state.devicesDescription.hasRemoteArchiveCapability == CombinedValue::All;

    ui->groupBoxMotionDetection->setVisible(remoteArchiveMdSupported);

    ui->remoteArchiveMotionDetectionCheckBox->setVisible(remoteArchiveMdSupported);
    if (remoteArchiveMdSupported)
    {
        check_box_utils::setupTristateCheckbox(
            ui->remoteArchiveMotionDetectionCheckBox,
            state.expert.remoteMotionDetectionEnabled);
        ::setReadOnly(ui->remoteArchiveMotionDetectionCheckBox, state.readOnly);
    }

    // Archive.

    check_box_utils::setupTristateCheckbox(
        ui->checkBoxPrimaryRecorder, state.expert.primaryRecordingDisabled);

    check_box_utils::setupTristateCheckbox(
        ui->checkBoxSecondaryRecorder, state.expert.secondaryRecordingDisabled);

    ui->checkBoxSecondaryRecorder->setVisible(hasDualStreaming);
    ui->checkBoxSecondaryRecorder->setEnabled(!state.expert.dualStreamingDisabled.valueOr(false)
        || !state.settingsOptimizationEnabled);

    ::setReadOnly(ui->checkBoxPrimaryRecorder, state.readOnly);
    ::setReadOnly(ui->checkBoxSecondaryRecorder, state.readOnly);

    // Media Streaming.

    if (state.devicesDescription.isUdpMulticastTransportAllowed == CombinedValue::All)
    {
        const auto index = ui->comboBoxTransport->findData(
            QVariant::fromValue(vms::api::RtpTransportType::multicast));

        if (index < 0)
        {
            ui->comboBoxTransport->addItem(
                tr("Multicast"),
                QVariant::fromValue(vms::api::RtpTransportType::multicast));
        }
    }
    else
    {
        const auto index = ui->comboBoxTransport->findData(
            QVariant::fromValue(vms::api::RtpTransportType::multicast));

        if (index >= 0)
            ui->comboBoxTransport->removeItem(index);
    }

    if (state.expert.rtpTransportType.hasValue())
    {
        const auto index = ui->comboBoxTransport->findData(
            QVariant::fromValue(state.expert.rtpTransportType()));

        NX_ASSERT(index > 0, "Unknown RTP stream type");
        ui->comboBoxTransport->setCurrentIndex(index);
    }
    else
    {
        ui->comboBoxTransport->setCurrentIndex(0/*multiple values*/);
    }

    fillProfiles(state, state.expert.forcedPrimaryProfile,
        ui->comboBoxPrimaryProfile, /*isPrimary*/ true);
    fillProfiles(state, state.expert.forcedSecondaryProfile,
        ui->comboBoxSecondaryProfile, /*isPrimary*/ false);

    ::setReadOnly(ui->comboBoxTransport, state.readOnly);

    ui->groupBoxWebPage->setVisible(
        state.devicesDescription.supportsWebPage == CombinedValue::All);
    if (state.expert.customWebPagePort.hasValue())
    {
        const bool customWebPagePortUsed = state.expert.customWebPagePort() > 0;
        ui->useAutoWebPagePortCheckBox->setTristate(false);
        ui->useAutoWebPagePortCheckBox->setChecked(!customWebPagePortUsed);
        ui->customWebPagePortSpinBox->setEnabled(customWebPagePortUsed);

        if (customWebPagePortUsed)
        {
            ui->customWebPagePortSpinBox->setMinimum(1);
            ui->customWebPagePortSpinBox->setValue(state.expert.customWebPagePort());
        }
        else
        {
            ui->customWebPagePortSpinBox->setMinimum(0);
            ui->customWebPagePortSpinBox->setValue(0);
        }
    }
    else
    {
        ui->useAutoWebPagePortCheckBox->setTristate(true);
        ui->useAutoWebPagePortCheckBox->setCheckState(Qt::PartiallyChecked);
        ui->customWebPagePortSpinBox->setEnabled(false);
        ui->customWebPagePortSpinBox->setMinimum(0);
        ui->customWebPagePortSpinBox->setValue(0);
    }
    ::setReadOnly(ui->useAutoWebPagePortCheckBox, state.readOnly);
    ::setReadOnly(ui->customWebPagePortSpinBox, state.readOnly);

    if (state.expert.customMediaPort.hasValue())
    {
        ui->customMediaPortSpinBox->setValue(
            state.expert.customMediaPort() == 0
                ? kDefaultRtspPort
                : state.expert.customMediaPortDisplayValue);
    }
    else
    {
        ui->customMediaPortSpinBox->setValue(ui->customMediaPortSpinBox->minimum());
    }

    if (state.devicesDescription.hasCustomMediaPort == CombinedValue::Some)
    {
        check_box_utils::setupTristateCheckbox(ui->autoMediaPortCheckBox, {});
    }
    else
    {
        ui->autoMediaPortCheckBox->setChecked(
            state.devicesDescription.hasCustomMediaPort == CombinedValue::None);
    }

    const bool isCustomMediaPortCapability =
        state.devicesDescription.hasCustomMediaPortCapability == CombinedValue::All
        && !state.readOnly;
    ui->mediaPortLabel->setEnabled(isCustomMediaPortCapability);
    ui->customMediaPortSpinBox->setEnabled(
        isCustomMediaPortCapability && !ui->autoMediaPortCheckBox->isChecked());
    ui->autoMediaPortCheckBox->setEnabled(isCustomMediaPortCapability);

    check_box_utils::setupTristateCheckbox(ui->trustCameraTimeCheckBox, state.expert.trustCameraTime);
    ::setReadOnly(ui->trustCameraTimeCheckBox, state.readOnly);

    // ONVIF Profile G remote archive automatic export.

    ui->remoteArchiveAutoImportGroupBox->setVisible(
        ini().enableRemoteArchiveSynchronization && remoteArchiveMdSupported);

    check_box_utils::setupTristateCheckbox(
        ui->remoteArchiveAutoImportCheckBox,
        state.expert.remoteArchiveAutoImportEnabled);

    // PTZ control block.
    ui->groupBoxPtzControl->setVisible(state.canSwitchPtzPresetTypes()
        || state.canForcePanTiltCapabilities()
        || state.canForceZoomCapability());

    { // PTZ Preset type selector.
        ui->preferredPtzPresetTypeWidget->setVisible(state.canSwitchPtzPresetTypes());
        const bool ptzPresetTypeSelected = state.expert.preferredPtzPresetType.hasValue();
        ui->prefferedPtzPresetTypeAutoRadioButton->setAutoExclusive(ptzPresetTypeSelected);
        ui->prefferedPtzPresetTypeNativeRadioButton->setAutoExclusive(ptzPresetTypeSelected);
        ui->prefferedPtzPresetTypeSystemRadioButton->setAutoExclusive(ptzPresetTypeSelected);
        ui->prefferedPtzPresetTypeAutoRadioButton->setChecked(isAutoPtzPreset(state));
        ui->prefferedPtzPresetTypeNativeRadioButton->setChecked(isNativePtzPreset(state));
        ui->prefferedPtzPresetTypeSystemRadioButton->setChecked(isSystemPtzPreset(state));
        ui->presetTypeLimitationsLabel->setVisible(isSystemPtzPreset(state));
        ::setReadOnly(ui->prefferedPtzPresetTypeSystemRadioButton, state.readOnly);
        ::setReadOnly(ui->prefferedPtzPresetTypeNativeRadioButton, state.readOnly);
        ::setReadOnly(ui->prefferedPtzPresetTypeAutoRadioButton, state.readOnly);
    }

    ui->forcedPtzWidget->setVisible(state.canForcePanTiltCapabilities()
        || state.canForceZoomCapability());

    ui->forcedPanTiltCheckBox->setVisible(state.canForcePanTiltCapabilities());
    ui->forcedZoomCheckBox->setVisible(state.canForceZoomCapability());

    if (state.canForcePanTiltCapabilities())
    {
        check_box_utils::setupTristateCheckbox(ui->forcedPanTiltCheckBox,
            state.expert.forcedPtzPanTiltCapability);
    }

    if (state.canForceZoomCapability())
    {
        check_box_utils::setupTristateCheckbox(ui->forcedZoomCheckBox,
            state.expert.forcedPtzZoomCapability);
    }

    ::setReadOnly(ui->forcedPanTiltCheckBox, state.readOnly);
    ::setReadOnly(ui->forcedZoomCheckBox, state.readOnly);

    // PTZ Sensitivity.

    const bool canAdjustPtzSensitivity = state.canAdjustPtzSensitivity();
    ui->ptzSensitivityGroupBox->setVisible(canAdjustPtzSensitivity);

    if (canAdjustPtzSensitivity)
    {
        check_box_utils::setupTristateCheckbox(ui->differentPtzSensitivitiesCheckBox,
            state.expert.ptzSensitivity.separate);

        const bool editable = state.expert.ptzSensitivity.separate.hasValue();

        const std::optional<qreal> pan = editable
            ? state.expert.ptzSensitivity.pan
            : std::optional<qreal>();

        spin_box_utils::setupSpinBox(ui->panSensitivitySpinBox, pan);

        ui->panSensitivitySlider->setValue(ptzSensitivityToSliderValue(
            pan.value_or(Ptz::kDefaultSensitivity)));

        ui->panSensitivityWidget->setEnabled(editable);

        const bool separateValues = state.expert.ptzSensitivity.separate.valueOr(false);
        if (separateValues)
        {
            spin_box_utils::setupSpinBox(
                ui->tiltSensitivitySpinBox, state.expert.ptzSensitivity.tilt);

            ui->tiltSensitivitySlider->setValue(ptzSensitivityToSliderValue(
                state.expert.ptzSensitivity.tilt.valueOr(Ptz::kDefaultSensitivity)));
        }

        ui->tiltSensitivityWidget->setVisible(separateValues);
        ui->panSensitivityLabel->setText(separateValues ? tr("Pan") : tr("Pan & Tilt"));

        ::setReadOnly(ui->differentPtzSensitivitiesCheckBox, state.readOnly);
        ::setReadOnly(ui->panSensitivitySpinBox, state.readOnly);
        ::setReadOnly(ui->panSensitivitySlider, state.readOnly);
        ::setReadOnly(ui->tiltSensitivitySpinBox, state.readOnly);
        ::setReadOnly(ui->tiltSensitivitySlider, state.readOnly);
    }

    // Logical ID.

    ui->logicalIdGroupBox->setVisible(state.isSingleCamera());
    if (state.isSingleCamera())
        ui->logicalIdSpinBox->setValue(state.singleCameraSettings.logicalId());

    const bool hasDuplicateLogicalIds =
        !state.singleCameraSettings.sameLogicalIdCameraNames.isEmpty();

    ui->logicalIdWarningLabel->setVisible(hasDuplicateLogicalIds);
    setWarningStyleOn(ui->logicalIdSpinBox, hasDuplicateLogicalIds);

    if (hasDuplicateLogicalIds)
    {
        const auto cameraList = lit(" <b>%1</b>")
            .arg(state.singleCameraSettings.sameLogicalIdCameraNames.join(lit(", ")));

        const auto errorMessage = tr("This ID is already used on the following %n cameras:", "",
            state.singleCameraSettings.sameLogicalIdCameraNames.size()) + cameraList;

        ui->logicalIdWarningLabel->setText(errorMessage);
    }

    ::setReadOnly(ui->logicalIdSpinBox, state.readOnly);
    ::setReadOnly(ui->generateLogicalIdButton, state.readOnly);
    ::setReadOnly(ui->resetLogicalIdButton, state.readOnly);

    // Reset to defaults.

    ui->restoreDefaultsButton->setEnabled(!state.isDefaultExpertSettings
        && state.supportsVideoStreamControl());
    ::setReadOnly(ui->restoreDefaultsButton, state.readOnly);

    // Alerts.

    ui->alertBlock->setAlerts(alerts(state));

    ui->motionImplicitlyDisabledAlertBar->setText(MotionStreamAlerts::implicitlyDisabledAlert(
        state.motion.streamAlert,
        !state.isSingleCamera()));

    // Force important layout change propagation.

    for (auto aligner: m_aligners)
        aligner->align();

    activateLayouts({
        ui->settingsGroupBox,
        ui->logicalIdGroupBox,
        ui->ptzSensitivityGroupBox,
        ui->rightWidget,
        ui->leftWidget});
}

QStringList CameraExpertSettingsWidget::alerts(const CameraSettingsDialogState& state) const
{
    QStringList alerts;
    if (state.expertAlerts.testFlag(CameraSettingsDialogState::ExpertAlert::cameraControlYielded))
    {
        alerts.push_back(tr("Quality and frame rate (FPS) settings in the Recording Schedule"
            " will become irrelevant."));
    }

    return alerts;
}

} // namespace nx::vms::client::desktop
