#include "camera_expert_settings_widget.h"
#include "ui_camera_expert_settings_widget.h"

#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/skin.h>
#include <ui/style/custom_style.h>
#include <ui/style/globals.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <nx/client/desktop/common/utils/checkbox_utils.h>
#include <nx/client/desktop/common/utils/combo_box_utils.h>
#include <nx/vms/api/types/rtp_types.h>
#include <nx/vms/api/types/motion_types.h>

using namespace nx::client::desktop;

namespace nx {
namespace client {
namespace desktop {

namespace {

void activateLayouts(const std::initializer_list<QWidget*>& widgets)
{
    for (auto widget: widgets)
    {
        if (auto layout = widget->layout())
            layout->activate();
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
    layout()->setContentsMargins(style::Metrics::kDefaultTopLevelMargins);

    NX_ASSERT(parent);
    auto scrollBar = new QnSnappedScrollBar(this);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());
    scrollBar->setUseMaximumSpace(true);

    CheckboxUtils::autoClearTristate(ui->checkBoxForceMotionDetection);
    CheckboxUtils::autoClearTristate(ui->secondStreamDisableCheckBox);

    setWarningStyle(ui->settingsWarningLabel);
    setWarningStyle(ui->bitrateIncreaseWarningLabel);
    setWarningStyle(ui->generalWarningLabel);
    setWarningStyle(ui->logicalIdWarningLabel);

    ui->settingsWarningLabel->setVisible(false);
    ui->bitrateIncreaseWarningLabel->setVisible(false);

    ui->comboBoxTransport->clear();
    ComboBoxUtils::insertMultipleValuesItem(ui->comboBoxTransport);
    ui->comboBoxTransport->addItem(tr("Auto", "Automatic RTP transport type"),
        QVariant::fromValue(vms::api::RtpTransportType::automatic));
    ui->comboBoxTransport->addItem(lit("TCP"), QVariant::fromValue(vms::api::RtpTransportType::tcp));
    ui->comboBoxTransport->addItem(lit("UDP"), QVariant::fromValue(vms::api::RtpTransportType::udp));

    ui->iconLabel->setPixmap(qnSkin->pixmap("theme/warning.png"));
    ui->iconLabel->setScaledContents(true);

    static const auto styleTemplateRaw = QString::fromLatin1(R"qss(
        .QWidget {
            border-style: solid;
            border-color: %1;
            border-width: 1px;
            border-radius: 2;
        })qss");

    const auto color = qnGlobals->errorTextColor();

    static const auto styleTemplate = styleTemplateRaw.arg(color.name(QColor::HexArgb));
    ui->warningContainer->setStyleSheet(styleTemplate);

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

    connect(ui->checkBoxDisableNativePtzPresets, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setNativePtzPresetsDisabled);

    connect(ui->logicalIdSpinBox, QnSpinboxIntValueChanged,
        store, &CameraSettingsDialogStore::setLogicalId);

    connect(ui->generateLogicalIdButton, &QPushButton::clicked,
        store, &CameraSettingsDialogStore::generateLogicalId);

    connect(ui->resetLogicalIdButton, &QPushButton::clicked, store,
        [store]() { store->setLogicalId({}); });

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
                store->setMotionStreamType(type.canConvert<vms::api::MotionStreamType>()
                    ? type.value<vms::api::MotionStreamType>()
                    : vms::api::MotionStreamType::primary);
            }
            else
            {
                store->setMotionStreamType(vms::api::MotionStreamType::automatic);
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

    setHelpTopic(ui->settingsDisableControlHint, Qn::CameraSettings_Expert_SettingsControl_Help);
    ui->settingsDisableControlHint->setHint(tr("Server will not change any cameras settings, "
        "it will receive and use camera stream as-is. "));

    setHelpTopic(ui->bitratePerGopHint, Qn::CameraSettings_Expert_SettingsControl_Help);
    ui->bitratePerGopHint->setHint(tr("Helps fix image quality issues on some cameras; "
        "for others will cause significant bitrate increase."));

    auto logicalIdHint = nx::client::desktop::HintButton::hintThat(ui->logicalIdGroupBox);
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
    // Camera settings.

    using CombinedValue = CameraSettingsDialogState::CombinedValue;
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

        CheckboxUtils::setupTristateCheckbox(
            ui->settingsDisableControlCheckBox,
            state.expert.cameraControlDisabled.hasValue() || !state.settingsOptimizationEnabled,
            controlDisabledForAll);
    }

    CheckboxUtils::setupTristateCheckbox(
        ui->bitratePerGopCheckBox,
        state.expert.useBitratePerGOP.hasValue(),
        state.expert.useBitratePerGOP.valueOr(false));

    // TODO: #vkutin #gdm Should we disable it too when camera control is disabled?
    ui->bitratePerGopCheckBox->setEnabled(state.settingsOptimizationEnabled
        && state.devicesDescription.hasPredefinedBitratePerGOP == CombinedValue::None);

    ui->bitrateIncreaseWarningLabel->setVisible(ui->bitratePerGopCheckBox->isEnabled()
        && ui->bitratePerGopCheckBox->isChecked());

    // Secondary Stream.

    const bool dualStreamingDisabledForAll = state.settingsOptimizationEnabled
        && state.expert.dualStreamingDisabled.valueOr(false);

    ui->secondStreamGroupBox->setVisible(hasDualStreaming);
    ui->secondStreamGroupBox->setEnabled(state.settingsOptimizationEnabled);

    if (hasDualStreaming)
    {
        CheckboxUtils::setupTristateCheckbox(
            ui->secondStreamDisableCheckBox,
            state.expert.dualStreamingDisabled.hasValue() || !state.settingsOptimizationEnabled,
            dualStreamingDisabledForAll);
    }

    // Motion override.

    ui->groupBoxMotionDetection->setVisible(
        state.devicesDescription.supportsMotionStreamOverride == CombinedValue::All);

    const bool allMotionStreamsOverridden =
        state.expert.motionStreamOverridden == CombinedValue::All;

    CheckboxUtils::setupTristateCheckbox(
        ui->checkBoxForceMotionDetection,
        state.expert.motionStreamOverridden != CombinedValue::Some,
        allMotionStreamsOverridden);

    ui->comboBoxForcedMotionStream->setVisible(allMotionStreamsOverridden);
    ui->comboBoxForcedMotionStream->clear();

    if (allMotionStreamsOverridden)
    {
        ComboBoxUtils::insertMultipleValuesItem(ui->comboBoxForcedMotionStream);
        ui->comboBoxForcedMotionStream->addItem(tr("Primary", "Primary stream for motion detection"),
            QVariant::fromValue(vms::api::MotionStreamType::primary));

        if (state.devicesDescription.hasDualStreamingCapability == CombinedValue::All)
        {
            ui->comboBoxForcedMotionStream->addItem(tr("Secondary",
                "Secondary stream for motion detection"),
                QVariant::fromValue(vms::api::MotionStreamType::secondary));
        }

        if (state.devicesDescription.hasRemoteArchiveCapability == CombinedValue::All)
        {
            ui->comboBoxForcedMotionStream->addItem(tr("Edge", "Edge stream for motion detection"),
                QVariant::fromValue(vms::api::MotionStreamType::edge));
        }

        if (state.expert.motionStreamType.hasValue())
        {
            const auto index = ui->comboBoxForcedMotionStream->findData(
                QVariant::fromValue(state.expert.motionStreamType()));

            NX_ASSERT(index > 0, Q_FUNC_INFO, "Unknown motion stream type");
            ui->comboBoxForcedMotionStream->setCurrentIndex(index);
        }
        else
        {
            ui->comboBoxForcedMotionStream->setCurrentIndex(0/*multiple values*/);
        }
    }

    ui->comboBoxForcedMotionStream->adjustSize();

    // Archive.

    CheckboxUtils::setupTristateCheckbox(
        ui->checkBoxPrimaryRecorder,
        state.expert.primaryRecordingDisabled.hasValue(),
        state.expert.primaryRecordingDisabled.valueOr(false));

    CheckboxUtils::setupTristateCheckbox(
        ui->checkBoxSecondaryRecorder,
        state.expert.secondaryRecordingDisabled.hasValue(),
        state.expert.secondaryRecordingDisabled.valueOr(false));

    ui->checkBoxSecondaryRecorder->setVisible(hasDualStreaming && !dualStreamingDisabledForAll);

    // Media Streaming.

    if (state.expert.rtpTransportType.hasValue())
    {
        const auto index = ui->comboBoxTransport->findData(
            QVariant::fromValue(state.expert.rtpTransportType()));

        NX_ASSERT(index > 0, Q_FUNC_INFO, "Unknown RTP stream type");
        ui->comboBoxTransport->setCurrentIndex(index);
    }
    else
    {
        ui->comboBoxTransport->setCurrentIndex(0/*multiple values*/);
    }

    // PTZ.

    // PTZ controls are visible if and only if at least one selected camera has PTZ presets capability.
    // PTZ controls are enabled if and only if at least one selected camera allows disabling
    //  native PTZ presets.

    const bool hasPtzPresets =
        state.devicesDescription.hasPtzPresets != CombinedValue::None;

    const bool canDisableNativePtzPresets =
        state.devicesDescription.canDisableNativePtzPresets != CombinedValue::None;

    ui->groupBoxPtzControl->setVisible(hasPtzPresets);
    ui->groupBoxPtzControl->setEnabled(canDisableNativePtzPresets);

    CheckboxUtils::setupTristateCheckbox(
        ui->checkBoxDisableNativePtzPresets,
        state.expert.nativePtzPresetsDisabled.hasValue() || !canDisableNativePtzPresets,
        state.expert.nativePtzPresetsDisabled.valueOr(false) && canDisableNativePtzPresets);

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

    // Reset to defaults.

    ui->restoreDefaultsButton->setEnabled(!state.isDefaultExpertSettings);

    // Force important layout change propagation.

    activateLayouts({
        ui->settingsGroupBox,
        ui->groupBoxArchive,
        ui->logicalIdGroupBox,
        ui->rightWidget,
        ui->leftWidget});
}

} // namespace desktop
} // namespace client
} // namespace nx
