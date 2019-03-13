#include "camera_expert_settings_widget.h"
#include "ui_camera_expert_settings_widget.h"

#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/skin.h>
#include <ui/style/custom_style.h>
#include <ui/style/globals.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <nx/vms/client/desktop/common/utils/check_box_utils.h>
#include <nx/vms/client/desktop/common/utils/combo_box_utils.h>
#include <nx/vms/client/desktop/common/widgets/hint_button.h>
#include <nx/vms/api/types/rtp_types.h>
#include <nx/vms/api/types/motion_types.h>

namespace nx::vms::client::desktop {

namespace {

void activateLayouts(const std::initializer_list<QWidget*>& widgets)
{
    for (auto widget: widgets)
    {
        if (auto layout = widget->layout())
            layout->activate();
    }
}

static const int kDefaultRtspPort = 554;

} // namespace

CameraExpertSettingsWidget::CameraExpertSettingsWidget(
    CameraSettingsDialogStore* store,
    QWidget* parent)
    :
    QWidget(parent),
    ui(new Ui::CameraExpertSettingsWidget)
{
    ui->setupUi(this);
    layout()->setContentsMargins(style::Metrics::kDefaultTopLevelMargins);

    NX_ASSERT(parent);
    auto scrollBar = new SnappedScrollBar(this);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());
    scrollBar->setUseMaximumSpace(true);

    check_box_utils::autoClearTristate(ui->checkBoxForceMotionDetection);
    check_box_utils::autoClearTristate(ui->secondStreamDisableCheckBox);
    check_box_utils::autoClearTristate(ui->forcedPanTiltCheckBox);
    check_box_utils::autoClearTristate(ui->forcedZoomCheckBox);
    check_box_utils::autoClearTristate(ui->trustCameraTimeCheckBox);

    setWarningStyle(ui->settingsWarningLabel);
    setWarningStyle(ui->bitrateIncreaseWarningLabel);
    setWarningStyle(ui->generalWarningLabel);
    setWarningStyle(ui->logicalIdWarningLabel);
    setWarningStyle(ui->presetTypeLimitationsLabel);

    ui->settingsWarningLabel->setVisible(false);
    ui->bitrateIncreaseWarningLabel->setVisible(false);

    ui->comboBoxTransport->clear();
    combo_box_utils::insertMultipleValuesItem(ui->comboBoxTransport);
    ui->comboBoxTransport->addItem(tr("Auto", "Automatic RTP transport type"),
        QVariant::fromValue(vms::api::RtpTransportType::automatic));
    ui->comboBoxTransport->addItem("TCP", QVariant::fromValue(vms::api::RtpTransportType::tcp));
    ui->comboBoxTransport->addItem("UDP", QVariant::fromValue(vms::api::RtpTransportType::udp));

    ui->preferredPtzPresetTypeComboBox->clear();
    combo_box_utils::insertMultipleValuesItem(ui->preferredPtzPresetTypeComboBox);
    ui->preferredPtzPresetTypeComboBox->addItem(tr("Auto", "Automatic PTZ preset type"),
        QVariant::fromValue(nx::core::ptz::PresetType::undefined));
    ui->preferredPtzPresetTypeComboBox->addItem(tr("System", "System PTZ preset type"),
        QVariant::fromValue(nx::core::ptz::PresetType::system));
    ui->preferredPtzPresetTypeComboBox->addItem(tr("Native", "Native PTZ preset type"),
        QVariant::fromValue(nx::core::ptz::PresetType::native));

    ui->iconLabel->setPixmap(qnSkin->pixmap("theme/warning.png"));
    ui->iconLabel->setScaledContents(true);

    setWarningFrame(ui->warningContainer);

    connect(store, &CameraSettingsDialogStore::stateChanged,
        this, &CameraExpertSettingsWidget::loadState);

    connect(ui->settingsDisableControlCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setCameraControlDisabled);

    connect(ui->secondStreamDisableCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setDualStreamingDisabled);

    connect(ui->bitratePerGopCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setUseBitratePerGOP);

    connect(ui->checkBoxPrimaryRecorder, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setPrimaryRecordingDisabled);

    connect(ui->checkBoxSecondaryRecorder, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setSecondaryRecordingDisabled);

    connect(ui->forcedPanTiltCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setForcedPtzPanTiltCapability);

    connect(ui->forcedZoomCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setForcedPtzZoomCapability);

    connect(ui->customMediaPortCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setCustomMediaPortUsed);

    connect(ui->customMediaPortSpinBox, QnSpinboxIntValueChanged,
        store, &CameraSettingsDialogStore::setCustomMediaPort);

    connect(ui->trustCameraTimeCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setTrustCameraTime);

    connect(ui->logicalIdSpinBox, QnSpinboxIntValueChanged,
        store, &CameraSettingsDialogStore::setLogicalId);

    connect(ui->generateLogicalIdButton, &QPushButton::clicked,
        store, &CameraSettingsDialogStore::generateLogicalId);

    connect(ui->resetLogicalIdButton, &QPushButton::clicked, store,
        [store]() { store->setLogicalId({}); });

    connect(ui->preferredPtzPresetTypeComboBox, QnComboboxCurrentIndexChanged, store,
        [this, store](int index)
        {
            store->setPreferredPtzPresetType(ui->preferredPtzPresetTypeComboBox->itemData(index)
                .value<nx::core::ptz::PresetType>());
        });

    connect(ui->comboBoxTransport, QnComboboxCurrentIndexChanged, store,
        [this, store](int index)
        {
            store->setRtpTransportType(
                ui->comboBoxTransport->itemData(index).value<vms::api::RtpTransportType>());
        });

    const auto setMotionStreamType =
        [this, store]()
        {
            if (ui->checkBoxForceMotionDetection->isChecked())
            {
                const auto type = ui->comboBoxForcedMotionStream->currentData();
                store->setForcedMotionStreamType(type.canConvert<vms::api::StreamIndex>()
                    ? type.value<vms::api::StreamIndex>()
                    : vms::api::StreamIndex::primary);
            }
            else
            {
                store->setForcedMotionStreamType(vms::api::StreamIndex::undefined);
            }
        };

    connect(ui->checkBoxForceMotionDetection, &QCheckBox::clicked,
        store, setMotionStreamType);

    connect(ui->comboBoxForcedMotionStream, QnComboboxCurrentIndexChanged,
        store, setMotionStreamType);

    connect(ui->restoreDefaultsButton, &QPushButton::clicked,
        store, &CameraSettingsDialogStore::resetExpertSettings);

    auto sizePolicy = ui->comboBoxForcedMotionStream->sizePolicy();
    sizePolicy.setRetainSizeWhenHidden(true);
    ui->comboBoxForcedMotionStream->setSizePolicy(sizePolicy);

    setHelpTopic(ui->secondStreamGroupBox, Qn::CameraSettings_SecondStream_Help);
    setHelpTopic(ui->settingsDisableControlCheckBox, Qn::CameraSettings_Expert_SettingsControl_Help);
    setHelpTopic(ui->checkBoxPrimaryRecorder, Qn::CameraSettings_Expert_DisableArchivePrimary_Help);
    setHelpTopic(ui->checkBoxSecondaryRecorder, Qn::CameraSettings_Expert_DisableArchivePrimary_Help);
    setHelpTopic(ui->groupBoxRTP, Qn::CameraSettings_Expert_Rtp_Help);

    setHelpTopic(ui->settingsDisableControlCheckBox, Qn::CameraSettings_Expert_SettingsControl_Help);
    ui->settingsDisableControlCheckBox->setHint(tr("Server will not change any cameras settings, "
        "it will receive and use camera stream as-is. "));

    setHelpTopic(ui->bitratePerGopCheckBox, Qn::CameraSettings_Expert_SettingsControl_Help);
    ui->bitratePerGopCheckBox->setHint(tr("Helps fix image quality issues on some cameras; "
        "for others will cause significant bitrate increase."));

    auto logicalIdHint = HintButton::hintThat(ui->logicalIdGroupBox);
    logicalIdHint->addHintLine(tr("Custom number that can be assigned to a camera "
        "for quick identification and access"));
    // TODO: #common #dkargin Fill in help topic when it is implemented.
    //logicalIdHint->setHelpTopic(Qn::)
}

CameraExpertSettingsWidget::~CameraExpertSettingsWidget()
{
}

void CameraExpertSettingsWidget::loadState(const CameraSettingsDialogState& state)
{
    // Disable unactual controls leaving only Logical Id for dts, wearable cameras or i/o modules.
    ui->leftWidget->setEnabled(state.supportsVideoStreamControl());
    ui->groupBoxArchive->setEnabled(state.supportsVideoStreamControl());
    ui->groupBoxRTP->setEnabled(state.supportsVideoStreamControl());

    const bool hasDualStreaming =
        state.devicesDescription.hasDualStreamingCapability != CombinedValue::None;

    const bool controlDisabledForAll = state.settingsOptimizationEnabled
        && state.expert.cameraControlDisabled.valueOr(false);

    const bool hasArecontCameras =
        state.devicesDescription.isArecontCamera != CombinedValue::None;

    ui->settingsGroupBox->setHidden(hasArecontCameras);
    if (!hasArecontCameras)
    {
        ui->settingsWarningLabel->setVisible(controlDisabledForAll);
        ui->settingsDisableControlCheckBox->setEnabled(state.settingsOptimizationEnabled);

        check_box_utils::setupTristateCheckbox(
            ui->settingsDisableControlCheckBox,
            state.expert.cameraControlDisabled.hasValue() || !state.settingsOptimizationEnabled,
            controlDisabledForAll);
    }

    check_box_utils::setupTristateCheckbox(ui->bitratePerGopCheckBox, state.expert.useBitratePerGOP);

    // TODO: #vkutin #gdm Should we disable it too when camera control is disabled?
    ui->bitratePerGopCheckBox->setEnabled(state.settingsOptimizationEnabled);

    ui->bitrateIncreaseWarningLabel->setVisible(ui->bitratePerGopCheckBox->isEnabled()
        && ui->bitratePerGopCheckBox->isChecked());

    ::setReadOnly(ui->settingsDisableControlCheckBox, state.readOnly);
    ::setReadOnly(ui->bitratePerGopCheckBox, state.readOnly);

    // Secondary Stream.

    const bool dualStreamingDisabledForAll = state.settingsOptimizationEnabled
        && state.expert.dualStreamingDisabled.valueOr(false);

    ui->secondStreamGroupBox->setVisible(hasDualStreaming);
    ui->secondStreamGroupBox->setEnabled(state.settingsOptimizationEnabled);

    if (hasDualStreaming)
    {
        check_box_utils::setupTristateCheckbox(
            ui->secondStreamDisableCheckBox,
            state.expert.dualStreamingDisabled.hasValue() || !state.settingsOptimizationEnabled,
            dualStreamingDisabledForAll);
    }

    ::setReadOnly(ui->secondStreamDisableCheckBox, state.readOnly);

    // Motion detection.

    const bool remoteArchiveMdSupported =
		state.devicesDescription.hasRemoteArchiveCapability == CombinedValue::All;
    const bool forcedSoftMdSupported =
		state.devicesDescription.supportsMotionStreamOverride == CombinedValue::All;

    ui->groupBoxMotionDetection->setVisible(remoteArchiveMdSupported || forcedSoftMdSupported);

    ui->remoteArchiveMotionDetectionCheckBox->setVisible(remoteArchiveMdSupported);
    if (remoteArchiveMdSupported)
    {
        check_box_utils::setupTristateCheckbox(
			ui->remoteArchiveMotionDetectionCheckBox,
			state.expert.remoteMotionDetectionEnabled);
        ::setReadOnly(ui->remoteArchiveMotionDetectionCheckBox, state.readOnly);
    }

    ui->forceSoftMotionDetectionWidget->setVisible(forcedSoftMdSupported);
    if (forcedSoftMdSupported)
	{
        const bool allMotionStreamsOverridden =
            state.expert.motionStreamOverridden == CombinedValue::All;

        check_box_utils::setupTristateCheckbox(
            ui->checkBoxForceMotionDetection,
            state.expert.motionStreamOverridden != CombinedValue::Some,
            allMotionStreamsOverridden);

        ui->comboBoxForcedMotionStream->setVisible(allMotionStreamsOverridden);
        ui->comboBoxForcedMotionStream->clear();

        if (allMotionStreamsOverridden)
        {
            combo_box_utils::insertMultipleValuesItem(ui->comboBoxForcedMotionStream);
            ui->comboBoxForcedMotionStream->addItem(
			    tr("Primary", "Primary stream for motion detection"),
                QVariant::fromValue(nx::vms::api::StreamIndex::primary));

            if (state.devicesDescription.hasDualStreamingCapability == CombinedValue::All)
            {
                ui->comboBoxForcedMotionStream->addItem(
				    tr("Secondary", "Secondary stream for motion detection"),
                    QVariant::fromValue(nx::vms::api::StreamIndex::secondary));
            }

            if (state.expert.forcedMotionStreamType.hasValue())
            {
                const auto index = ui->comboBoxForcedMotionStream->findData(
                    QVariant::fromValue(state.expert.forcedMotionStreamType()));

                NX_ASSERT(index > 0, "Unknown motion stream type");
                ui->comboBoxForcedMotionStream->setCurrentIndex(index);
            }
            else
            {
                ui->comboBoxForcedMotionStream->setCurrentIndex(/*multiple values*/ 0);
            }

        }

        ui->comboBoxForcedMotionStream->adjustSize();

        ::setReadOnly(ui->checkBoxForceMotionDetection, state.readOnly);
        ::setReadOnly(ui->comboBoxForcedMotionStream, state.readOnly);
	}

    // Archive.

    check_box_utils::setupTristateCheckbox(
        ui->checkBoxPrimaryRecorder, state.expert.primaryRecordingDisabled);

    check_box_utils::setupTristateCheckbox(
        ui->checkBoxSecondaryRecorder, state.expert.secondaryRecordingDisabled);

    ui->checkBoxSecondaryRecorder->setVisible(hasDualStreaming && !dualStreamingDisabledForAll);

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
                tr("UDP Multicast"),
                QVariant::fromValue(vms::api::RtpTransportType::multicast));
        }
    }
    else
    {
        const auto index = ui->comboBoxTransport->findData(
            QVariant::fromValue(vms::api::RtpTransportType::multicast));

        const auto currentIndex = ui->comboBoxTransport->currentIndex();
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

    ::setReadOnly(ui->comboBoxTransport, state.readOnly);

    ui->customMediaPortSpinBox->setValue(state.expert.customMediaPortDisplayValue);
    if (state.expert.customMediaPort.hasValue())
    {
        const bool isCustomMediaPort = state.expert.customMediaPort() != 0;
        ui->customMediaPortSpinBox->setEnabled(isCustomMediaPort);
        ui->customMediaPortCheckBox->setChecked(isCustomMediaPort);
    }
    else
    {
        check_box_utils::setupTristateCheckbox(ui->customMediaPortCheckBox, {});
        ui->customMediaPortSpinBox->setEnabled(false);
    }
    ::setReadOnly(ui->customMediaPortWidget, state.readOnly);
    ui->customMediaPortWidget->setEnabled(
        state.devicesDescription.hasCustomMediaPortCapability == CombinedValue::All);

    check_box_utils::setupTristateCheckbox(ui->trustCameraTimeCheckBox, state.expert.trustCameraTime);
    ::setReadOnly(ui->trustCameraTimeCheckBox, state.readOnly);

    // PTZ.

    // Preset types are now displayed only if they are editable.
    // Greyed out uneditable preset types are no longer displayed.

    ui->groupBoxPtzControl->setVisible(state.canSwitchPtzPresetTypes()
        || state.canForcePanTiltCapabilities()
        || state.canForceZoomCapability());

    ui->preferredPtzPresetTypeWidget->setVisible(state.canSwitchPtzPresetTypes());
    ui->forcedPtzWidget->setVisible(state.canForcePanTiltCapabilities()
        || state.canForceZoomCapability());

    ui->forcedPanTiltCheckBox->setVisible(state.canForcePanTiltCapabilities());
    ui->forcedZoomCheckBox->setVisible(state.canForceZoomCapability());

    ui->presetTypeLimitationsLabel->setVisible(state.canSwitchPtzPresetTypes()
        && state.expert.preferredPtzPresetType.hasValue()
        && state.expert.preferredPtzPresetType() == nx::core::ptz::PresetType::system);

    if (state.canSwitchPtzPresetTypes())
    {
        if (state.expert.preferredPtzPresetType.hasValue())
        {
            const auto index = ui->preferredPtzPresetTypeComboBox->findData(
                QVariant::fromValue(state.expert.preferredPtzPresetType()));

            NX_ASSERT(index > 0, "Unknown PTZ preset type");
            ui->preferredPtzPresetTypeComboBox->setCurrentIndex(index);
        }
        else
        {
            ui->preferredPtzPresetTypeComboBox->setCurrentIndex(0/*multiple values*/);
        }
    }

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

    ::setReadOnly(ui->preferredPtzPresetTypeComboBox, state.readOnly);
    ::setReadOnly(ui->forcedPanTiltCheckBox, state.readOnly);
    ::setReadOnly(ui->forcedZoomCheckBox, state.readOnly);

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

    // Force important layout change propagation.

    activateLayouts({
        ui->settingsGroupBox,
        ui->groupBoxArchive,
        ui->logicalIdGroupBox,
        ui->rightWidget,
        ui->leftWidget});
}

} // namespace nx::vms::client::desktop
