#include "legacy_expert_settings_widget.h"
#include "ui_legacy_expert_settings_widget.h"

#include <QtCore/QtGlobal>
#include <QtWidgets/QListView>

#include <ui/style/skin.h>
#include <ui/style/custom_style.h>
#include <ui/style/globals.h>

#include <common/common_module.h>

#include <client/client_globals.h>

#include <api/global_settings.h>
#include <core/resource/resource_type.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_resource.h>

#include <nx/vms/client/desktop/common/utils/check_box_utils.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <ui/common/read_only.h>
using namespace nx::vms::client::desktop;

namespace {

// Native ptz presets can be disabled if they are supported and nx presets are supported too.
bool canSwitchPresetTypes(const QnVirtualCameraResourcePtr& camera)
{
    const auto caps = camera->getPtzCapabilities();
    return caps.testFlag(Ptz::NativePresetsPtzCapability)
        && !caps.testFlag(Ptz::NoNxPresetsPtzCapability);
}

static const int kAutoPresetsIndex = 0;
static const int kSystemPresetsIndex = 1;
static const int kNativePresetsIndex = 2;

nx::core::ptz::PresetType presetTypeByIndex(int index)
{
    switch (index)
    {
        case kSystemPresetsIndex: return nx::core::ptz::PresetType::system;
        case kNativePresetsIndex: return nx::core::ptz::PresetType::native;
        default: return nx::core::ptz::PresetType::undefined;
    }
}

} // namespace

namespace nx::vms::client::desktop {

LegacyExpertSettingsWidget::LegacyExpertSettingsWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::LegacyExpertSettingsWidget)
{
    ui->setupUi(this);

    NX_ASSERT(parent);
    SnappedScrollBar* scrollBar = new SnappedScrollBar(window());
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());
    scrollBar->setUseMaximumSpace(true);

    check_box_utils::autoClearTristate(ui->checkBoxForceMotionDetection);
    check_box_utils::autoClearTristate(ui->secondStreamDisableCheckBox);

    setWarningStyle(ui->settingsWarningLabel);
    setWarningStyle(ui->bitrateIncreaseWarningLabel);
    setWarningStyle(ui->generalWarningLabel);
    setWarningStyle(ui->logicalIdWarningLabel);
    setWarningStyle(ui->presetTypeLimitationsLabel);

    ui->presetTypeLimitationsLabel->hide();

    ui->settingsWarningLabel->setVisible(false);
    ui->bitrateIncreaseWarningLabel->setVisible(false);

    ui->iconLabel->setPixmap(qnSkin->pixmap("theme/warning.png"));
    ui->iconLabel->setScaledContents(true);

    static const auto styleTemplateRaw = QString::fromLatin1(R"(.QWidget {
        border-style: solid;
        border-color: %1;
        border-width: 1px;
        border-radius: 2;
    })");

    const auto color = qnGlobals->errorTextColor();

    static const auto styleTemplate = styleTemplateRaw.arg(color.name(QColor::HexArgb));
    ui->warningContainer->setStyleSheet(styleTemplate);

    connect(ui->settingsDisableControlCheckBox, &QCheckBox::toggled,
        ui->secondStreamGroupBox, &QGroupBox::setDisabled);
    connect(ui->settingsDisableControlCheckBox, &QCheckBox::toggled,
        this, &LegacyExpertSettingsWidget::updateControlBlock);
    connect(ui->bitratePerGopCheckBox, &QCheckBox::toggled,
        this, &LegacyExpertSettingsWidget::updateControlBlock);
    connect(qnGlobalSettings, &QnGlobalSettings::cameraSettingsOptimizationChanged,
        this, &LegacyExpertSettingsWidget::updateControlBlock);
    updateControlBlock();

    connect(ui->restoreDefaultsButton, SIGNAL(clicked()), this, SLOT(at_restoreDefaultsButton_clicked()));

    connect(ui->settingsDisableControlCheckBox, SIGNAL(stateChanged(int)), this, SLOT(at_dataChanged()));
    connect(ui->checkBoxPrimaryRecorder, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));
    connect(ui->bitratePerGopCheckBox, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));
    connect(ui->checkBoxSecondaryRecorder, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));
    connect(ui->comboBoxTransport, SIGNAL(currentIndexChanged(int)), this, SLOT(at_dataChanged()));

    connect(
        ui->preferredPtzPresetTypeComboBox, QnComboboxCurrentIndexChanged,
        this, &LegacyExpertSettingsWidget::at_dataChanged);

    connect(
        ui->preferredPtzPresetTypeComboBox, QnComboboxCurrentIndexChanged,
        this, &LegacyExpertSettingsWidget::at_preferredPresetTypeChanged);

    connect(
        ui->forcedPanTiltCheckBox, &QCheckBox::toggled,
        this, &LegacyExpertSettingsWidget::at_dataChanged);

    connect(
        ui->forcedZoomCheckBox, &QCheckBox::toggled,
        this, &LegacyExpertSettingsWidget::at_dataChanged);

    connect(ui->secondStreamDisableCheckBox, &QCheckBox::stateChanged,
        this, &LegacyExpertSettingsWidget::at_dataChanged);

    connect(
        ui->checkBoxForceMotionDetection, &QCheckBox::stateChanged,
        [this](int state)
        {
            ui->comboBoxForcedMotionStream->setEnabled(
                static_cast<Qt::CheckState>(state) == Qt::Checked
                && ui->checkBoxForceMotionDetection->isEnabled());
        });

    connect(
        ui->checkBoxForceMotionDetection, &QCheckBox::toggled,
        this, &LegacyExpertSettingsWidget::at_dataChanged);

    connect(
        ui->comboBoxForcedMotionStream, QnComboboxCurrentIndexChanged,
        this, &LegacyExpertSettingsWidget::at_dataChanged);

    connect(
        ui->logicalIdSpinBox, QnSpinboxIntValueChanged,
        this, &LegacyExpertSettingsWidget::at_dataChanged);
    connect(
        ui->resetLogicalIdButton, &QPushButton::clicked,
        this, [this]() {ui->logicalIdSpinBox->setValue(0);});
    connect(
        ui->generateLogicalIdButton, &QPushButton::clicked,
        this, &LegacyExpertSettingsWidget::at_generateLogicalId);

    setHelpTopic(ui->secondStreamGroupBox, Qn::CameraSettings_SecondStream_Help);
    setHelpTopic(ui->settingsDisableControlCheckBox, Qn::CameraSettings_Expert_SettingsControl_Help);
    setHelpTopic(ui->checkBoxPrimaryRecorder, Qn::CameraSettings_Expert_DisableArchivePrimary_Help);
    setHelpTopic(ui->checkBoxSecondaryRecorder, Qn::CameraSettings_Expert_DisableArchivePrimary_Help);
    setHelpTopic(ui->groupBoxRTP, Qn::CameraSettings_Expert_Rtp_Help);

    // TODO: Help topic for PTZ stuff.

    ui->settingsDisableControlHint->setHint(
        tr("Server will not change any cameras settings, it will receive and use camera stream as-is."));
    setHelpTopic(ui->settingsDisableControlHint, Qn::CameraSettings_Expert_SettingsControl_Help);

    ui->bitratePerGopHint->setHint(tr("Helps fix image quality issues on some cameras; for others will cause significant bitrate increase."));
    setHelpTopic(ui->bitratePerGopHint, Qn::CameraSettings_Expert_CalculateBitratePerGop);

    auto logicalIdHint = nx::vms::client::desktop::HintButton::hintThat(ui->logicalIdGroupBox);
    logicalIdHint->addHintLine(tr("Custom number that can be assigned to a camera for quick identification and access"));
    setHelpTopic(logicalIdHint, Qn::CameraSettings_Expert_LogicalId_Help);
}

LegacyExpertSettingsWidget::~LegacyExpertSettingsWidget()
{

}

void LegacyExpertSettingsWidget::updateFromResources(const QnVirtualCameraResourceList &cameras)
{
    QScopedValueRollback<bool> guard(m_updating, true);

    boost::optional<bool> isDualStreamingDisabled;
    bool sameIsDualStreamingDisabled = true;

    bool sameControlState = true;
    bool controlDisabled = false;

    int arecontCamerasCount = 0;
    m_hasDualStreaming = false;
    m_hasRemoteArchiveCapability = false;

    bool isFirstControl = true;

    int primaryRecorderDisabled = -1;
    int bitratePerGopPresent = -1;
    int secondaryRecorderDisabled = -1;
    bool samePrimaryRec = true;
    bool sameBitratePerGOP = true;
    bool sameSecRec = true;

    bool sameRtpTransport = true;
    QString rtpTransport;

    bool sameMdPolicies = true;
    QString mdPolicy;

    const int kPrimaryStreamMdIndex = 0;
    const int kSecondaryStreamMdIndex = 1;
    const int kEdgeStreamMdIndex = 2;

    int forcedMotionStreamIndex = -1;
    bool allCamerasSupportForceMotion = true;

    int supportBothPresetTypeCount = 0;
    int preferSystemPresetsCount = 0;
    int preferNativePresetsCount = 0;

    int modifiablePtzCapabilitiesCount = 0;
    int addedPanTiltCount = 0;
    int addedZoomCount = 0;

    int camCnt = 0;
    for (const auto& camera: cameras)
    {
        if (isArecontCamera(camera))
            arecontCamerasCount++;
        if (!camera->supportedMotionType().testFlag(Qn::MotionType::MT_SoftwareGrid))
            allCamerasSupportForceMotion = false;

        m_hasDualStreaming |= camera->hasDualStreamingInternal();
        m_hasRemoteArchiveCapability |= camera->hasCameraCapabilities(Qn::RemoteArchiveCapability);

        if (camera->hasDualStreamingInternal())
        {
            if (isDualStreamingDisabled.is_initialized())
            {
                sameIsDualStreamingDisabled &=
                    *isDualStreamingDisabled == camera->isDualStreamingDisabled();
            }
            else
            {
                isDualStreamingDisabled = camera->isDualStreamingDisabled();
            }
        }

        if (!isArecontCamera(camera)) {
            if (isFirstControl) {
                isFirstControl = false;
                controlDisabled = camera->isCameraControlDisabled();
            } else {
                sameControlState &= camera->isCameraControlDisabled() == controlDisabled;
            }
        }

        int secRecDisabled = camera->getProperty(QnMediaResource::dontRecordSecondaryStreamKey()).toInt();
        if (secondaryRecorderDisabled == -1)
            secondaryRecorderDisabled = secRecDisabled;
        else if (secondaryRecorderDisabled != secRecDisabled)
            sameSecRec = false;

        int primaryRecDisabled = camera->getProperty(QnMediaResource::dontRecordPrimaryStreamKey()).toInt();
        if (primaryRecorderDisabled == -1)
            primaryRecorderDisabled = primaryRecDisabled;
        else if (primaryRecorderDisabled != primaryRecDisabled)
            samePrimaryRec = false;

        int bitratePerGop = camera->useBitratePerGop() ? 1 : 0;
        if (bitratePerGopPresent == -1)
            bitratePerGopPresent = bitratePerGop;
        else if (bitratePerGopPresent != bitratePerGop)
            sameBitratePerGOP = false;

        QString camRtpTransport = camera->getProperty(QnMediaResource::rtpTransportKey());
        if (camRtpTransport != rtpTransport && camCnt > 0)
            sameRtpTransport = false;
        rtpTransport = camRtpTransport;

        if (canSwitchPresetTypes(camera))
        {
            ++supportBothPresetTypeCount;
            const auto preferredPtzPresetType = camera->userPreferredPtzPresetType();

            if (preferredPtzPresetType == nx::core::ptz::PresetType::system)
                ++preferSystemPresetsCount;

            if (preferredPtzPresetType == nx::core::ptz::PresetType::native)
                ++preferNativePresetsCount;
        }

        if (camera->isUserAllowedToModifyPtzCapabilities())
            ++modifiablePtzCapabilitiesCount;

        const auto capabilitiesAddedByUser = camera->ptzCapabilitiesAddedByUser();
        if (capabilitiesAddedByUser & Ptz::ContinuousPanTiltCapabilities)
            ++addedPanTiltCount;

        if (capabilitiesAddedByUser & Ptz::ContinuousZoomCapability)
            ++addedZoomCount;

        camCnt++;
    }

    ui->secondStreamGroupBox->setVisible(m_hasDualStreaming);

    if (m_hasDualStreaming)
    {
        check_box_utils::setupTristateCheckbox(
            ui->secondStreamDisableCheckBox,
            sameIsDualStreamingDisabled,
            isDualStreamingDisabled ? *isDualStreamingDisabled : false);
    }

    if (samePrimaryRec)
        ui->checkBoxPrimaryRecorder->setChecked(primaryRecorderDisabled);
    else
        ui->checkBoxPrimaryRecorder->setCheckState(Qt::PartiallyChecked);

    if (sameBitratePerGOP)
        ui->bitratePerGopCheckBox->setChecked(bitratePerGopPresent);
    else
        ui->bitratePerGopCheckBox->setCheckState(Qt::PartiallyChecked);

    ui->checkBoxSecondaryRecorder->setEnabled(m_hasDualStreaming);
    if (m_hasDualStreaming)
    {
        if (sameSecRec)
            ui->checkBoxSecondaryRecorder->setChecked(secondaryRecorderDisabled);
        else
            ui->checkBoxSecondaryRecorder->setCheckState(Qt::PartiallyChecked);
    }
    else
    {
        ui->checkBoxSecondaryRecorder->setChecked(false);
    }

    if (rtpTransport.isEmpty())
        ui->comboBoxTransport->setCurrentIndex(0);
    else if (sameRtpTransport)
        ui->comboBoxTransport->setCurrentText(rtpTransport);
    else
        ui->comboBoxTransport->setCurrentIndex(-1);

    ui->checkBoxForceMotionDetection->setEnabled(allCamerasSupportForceMotion);

    ui->comboBoxForcedMotionStream->setEnabled(
        ui->checkBoxForceMotionDetection->checkState() == Qt::Checked
        && ui->checkBoxForceMotionDetection->isEnabled());

    updateControlBlock();
    ui->settingsGroupBox->setVisible(arecontCamerasCount != cameras.size());
    ui->settingsDisableControlCheckBox->setTristate(!sameControlState);
    if (sameControlState)
        ui->settingsDisableControlCheckBox->setChecked(controlDisabled);
    else
        ui->settingsDisableControlCheckBox->setCheckState(Qt::PartiallyChecked);

    const bool canSwitchPresetTypeOnCameras = supportBothPresetTypeCount != 0;
    const bool canModifyPtzCapabilities = modifiablePtzCapabilitiesCount != 0;

    ui->groupBoxPtzControl->setEnabled(canSwitchPresetTypeOnCameras || canModifyPtzCapabilities);
    ui->groupBoxPtzControl->setVisible(canSwitchPresetTypeOnCameras || canModifyPtzCapabilities);

    ui->preferredPtzPresetTypeWidget->setVisible(canSwitchPresetTypeOnCameras);
    ui->preferredPtzPresetTypeWidget->setEnabled(canSwitchPresetTypeOnCameras);
    ui->preferredPtzPresetTypeComboBox->setEnabled(canSwitchPresetTypeOnCameras);

    if (canSwitchPresetTypeOnCameras)
    {
        if (preferSystemPresetsCount == supportBothPresetTypeCount)
            ui->preferredPtzPresetTypeComboBox->setCurrentIndex(kSystemPresetsIndex);
        else if (preferNativePresetsCount == supportBothPresetTypeCount)
            ui->preferredPtzPresetTypeComboBox->setCurrentIndex(kNativePresetsIndex);
        else if (preferSystemPresetsCount == 0 && preferNativePresetsCount == 0)
            ui->preferredPtzPresetTypeComboBox->setCurrentIndex(kAutoPresetsIndex);
        else
            ui->preferredPtzPresetTypeComboBox->setCurrentIndex(-1);
    }

    ui->forcedPtzWidget->setVisible(canModifyPtzCapabilities);
    ui->forcedPtzWidget->setEnabled(canModifyPtzCapabilities);
    ui->forcedPanTiltCheckBox->setEnabled(canModifyPtzCapabilities);
    ui->forcedZoomCheckBox->setEnabled(canModifyPtzCapabilities);

    check_box_utils::setupTristateCheckbox(
        ui->forcedPanTiltCheckBox,
        cameras.size() == addedPanTiltCount || addedPanTiltCount == 0,
        cameras.size() == addedPanTiltCount);

    check_box_utils::setupTristateCheckbox(
        ui->forcedZoomCheckBox,
        cameras.size() == addedZoomCount || addedZoomCount == 0,
        cameras.size() == addedZoomCount);

    const bool canSetupVideoStream = std::all_of(cameras.cbegin(), cameras.cend(),
        [](const QnVirtualCameraResourcePtr& camera)
        {
            return !camera->isDtsBased()
                && !camera->hasFlags(Qn::wearable_camera)
                && camera->hasVideo();
        });

    ui->leftWidget->setEnabled(canSetupVideoStream);
    ui->groupBoxArchive->setEnabled(canSetupVideoStream);
    ui->groupBoxRTP->setEnabled(canSetupVideoStream);
    ui->restoreDefaultsButton->setEnabled(canSetupVideoStream);

    m_currentCameraId = cameras.front()->getId();
    ui->logicalIdSpinBox->setValue(cameras.size() == 1
        ? cameras.front()->logicalId() : 0);
    ui->logicalIdGroupBox->setEnabled(cameras.size() == 1);
}

void LegacyExpertSettingsWidget::submitToResources(const QnVirtualCameraResourceList& cameras)
{
    bool disableControls = ui->settingsDisableControlCheckBox->checkState() == Qt::Checked;
    bool enableControls = ui->settingsDisableControlCheckBox->checkState() == Qt::Unchecked;
    bool globalControlEnabled = qnGlobalSettings->isCameraSettingsOptimizationEnabled();

    for (const auto& camera: cameras)
    {
        if (globalControlEnabled && !isArecontCamera(camera))
        {
            if (disableControls)
                camera->setCameraControlDisabled(true);
            else if (enableControls)
                camera->setCameraControlDisabled(false);
        }

        if (globalControlEnabled && enableControls && camera->hasDualStreamingInternal())
        {
            if (ui->secondStreamDisableCheckBox->checkState() != Qt::PartiallyChecked)
                camera->setDisableDualStreaming(
                    ui->secondStreamDisableCheckBox->checkState() == Qt::Checked);
        }

        if (ui->checkBoxPrimaryRecorder->checkState() != Qt::PartiallyChecked)
            camera->setProperty(QnMediaResource::dontRecordPrimaryStreamKey(), ui->checkBoxPrimaryRecorder->isChecked() ? lit("1") : lit("0"));
        if (ui->bitratePerGopCheckBox->checkState() != Qt::PartiallyChecked)
            camera->setProperty(ResourcePropertyKey::kBitratePerGOP, ui->bitratePerGopCheckBox->isChecked() ? lit("1") : lit("0"));
        if (ui->checkBoxSecondaryRecorder->checkState() != Qt::PartiallyChecked && camera->hasDualStreamingInternal())
            camera->setProperty(QnMediaResource::dontRecordSecondaryStreamKey(), ui->checkBoxSecondaryRecorder->isChecked() ? lit("1") : lit("0"));

        if (ui->comboBoxTransport->currentIndex() >= 0) {
            QString txt = ui->comboBoxTransport->currentText();
            if (txt.toLower() == lit("auto"))
                txt.clear();
            camera->setProperty(QnMediaResource::rtpTransportKey(), txt);
        }

        const auto preferredPtzPresetTypeIndex = ui->preferredPtzPresetTypeComboBox->currentIndex();
        if (preferredPtzPresetTypeIndex != -1 && canSwitchPresetTypes(camera))
        {
            camera->setUserPreferredPtzPresetType(presetTypeByIndex(preferredPtzPresetTypeIndex));
        }

        if (camera->isUserAllowedToModifyPtzCapabilities())
        {
            auto userAddedPtzCapabilities = camera->ptzCapabilitiesAddedByUser();
            if (ui->forcedPanTiltCheckBox->checkState() == Qt::Checked)
                userAddedPtzCapabilities |= Ptz::ContinuousPanTiltCapabilities;
            else if (ui->forcedPanTiltCheckBox->checkState() == Qt::Unchecked)
                userAddedPtzCapabilities &= ~Ptz::ContinuousPanTiltCapabilities;

            if (ui->forcedZoomCheckBox->checkState() == Qt::Checked)
                userAddedPtzCapabilities |= Ptz::ContinuousZoomCapability;
            else if (ui->forcedZoomCheckBox->checkState() == Qt::Unchecked)
                userAddedPtzCapabilities &= ~Ptz::ContinuousZoomCapability;

            camera->setPtzCapabilitiesAddedByUser(userAddedPtzCapabilities);
        }

        if (ui->logicalIdGroupBox->isEnabled())
        {
            if (ui->logicalIdSpinBox->value() > 0)
                camera->setLogicalId(ui->logicalIdSpinBox->value());
            else
                camera->setLogicalId(0);
        }
    }
}

bool LegacyExpertSettingsWidget::isArecontCamera(const QnVirtualCameraResourcePtr &camera) const {
    QnResourceTypePtr cameraType = qnResTypePool->getResourceType(camera->getTypeId());
    return cameraType && cameraType->getManufacturer() == lit("ArecontVision");
}

bool LegacyExpertSettingsWidget::isMdPolicyAllowedForCamera(const QnVirtualCameraResourcePtr& camera, const QString& mdPolicy) const
{
    return false;
}

int LegacyExpertSettingsWidget::generateFreeLogicalId() const
{
    auto cameras = commonModule()->resourcePool()->getAllCameras();
    std::set<int> usedValues;
    for (const auto& camera: cameras)
    {
        if (camera->getId() == m_currentCameraId)
            continue;
        const auto id = camera->logicalId();
        if (id > 0)
            usedValues.insert(id);
    }

    int previousValue = 0;
    for (auto value: usedValues)
    {
        if (value > previousValue + 1)
            break;
        previousValue = value;
    }
    return previousValue + 1;
}

void LegacyExpertSettingsWidget::at_generateLogicalId()
{
    ui->logicalIdSpinBox->setValue(generateFreeLogicalId());
}

void LegacyExpertSettingsWidget::updateLogicalIdControls()
{
    auto duplicateCameras = commonModule()->resourcePool()->getAllCameras().filtered(
        [this](const QnVirtualCameraResourcePtr& camera)
        {
            const int logicalId = camera->logicalId();
            return logicalId > 0
                && logicalId == ui->logicalIdSpinBox->value()
                && camera->getId() != m_currentCameraId;
        });

    QStringList cameraNames;
    for (const auto& camera : duplicateCameras)
        cameraNames << camera->getName();
    const auto errorMessage = tr(
            "This ID is already used on the following %n cameras: %1",
            "",
            cameraNames.size())
        .arg(lit("<b>%1</b>").arg(cameraNames.join(lit(", "))));

    ui->logicalIdWarningLabel->setVisible(!duplicateCameras.isEmpty());
    ui->logicalIdWarningLabel->setText(errorMessage);

    setWarningStyleOn(ui->logicalIdSpinBox, !duplicateCameras.isEmpty());
}

void LegacyExpertSettingsWidget::at_dataChanged()
{
    updateLogicalIdControls();
    if (!m_updating)
        emit dataChanged();
}

void LegacyExpertSettingsWidget::at_preferredPresetTypeChanged(int index)
{
    ui->presetTypeLimitationsLabel->setVisible(index == kSystemPresetsIndex);
}

bool LegacyExpertSettingsWidget::areDefaultValues() const
{
    if (ui->bitratePerGopCheckBox->isEnabled() && ui->bitratePerGopCheckBox->isChecked())
        return false;

    return ui->settingsDisableControlCheckBox->checkState() == Qt::Unchecked
        && ui->checkBoxPrimaryRecorder->checkState() == Qt::Unchecked
        && ui->checkBoxSecondaryRecorder->checkState() == Qt::Unchecked
        && ui->comboBoxTransport->currentIndex() == 0
        && ui->checkBoxForceMotionDetection->checkState() == Qt::Unchecked
        && ui->preferredPtzPresetTypeComboBox->currentIndex() == kAutoPresetsIndex
        && ui->secondStreamDisableCheckBox->checkState() == Qt::Unchecked
        && ui->forcedZoomCheckBox->checkState() == Qt::Unchecked
        && ui->forcedPanTiltCheckBox->checkState() == Qt::Unchecked;
}

void LegacyExpertSettingsWidget::at_restoreDefaultsButton_clicked()
{
    ui->settingsDisableControlCheckBox->setCheckState(Qt::Unchecked);
    ui->secondStreamDisableCheckBox->setChecked(false);
    ui->checkBoxPrimaryRecorder->setChecked(false);
    ui->bitratePerGopCheckBox->setChecked(false);
    ui->checkBoxSecondaryRecorder->setChecked(false);
    ui->comboBoxTransport->setCurrentIndex(0);
    ui->checkBoxForceMotionDetection->setCheckState(Qt::Unchecked);
    ui->comboBoxForcedMotionStream->setCurrentIndex(0);
    ui->preferredPtzPresetTypeComboBox->setCurrentIndex(kAutoPresetsIndex);
    ui->logicalIdSpinBox->setValue(0);
    ui->forcedZoomCheckBox->setCheckState(Qt::Unchecked);
    ui->forcedPanTiltCheckBox->setCheckState(Qt::Unchecked);
}

void LegacyExpertSettingsWidget::updateControlBlock()
{
    const auto globalControlEnabled = qnGlobalSettings->isCameraSettingsOptimizationEnabled();
    ui->settingsDisableControlCheckBox->setEnabled(globalControlEnabled);

    ui->settingsWarningLabel->setVisible(globalControlEnabled &&
        ui->settingsDisableControlCheckBox->isChecked());

    ui->bitrateIncreaseWarningLabel->setVisible(globalControlEnabled &&
        ui->bitratePerGopCheckBox->isChecked());

    ui->settingsGroupBox->layout()->activate();
    ui->leftWidget->layout()->activate();
}

bool LegacyExpertSettingsWidget::isSecondStreamEnabled() const
{
    if (ui->settingsDisableControlCheckBox->checkState() != Qt::Unchecked)
        return true;

    if (!ui->secondStreamGroupBox->isEnabled())
        return true;

    return ui->secondStreamDisableCheckBox->checkState() == Qt::Unchecked;
}

void LegacyExpertSettingsWidget::setSecondStreamEnabled(bool value)
{
    if (ui->settingsDisableControlCheckBox->checkState() != Qt::Unchecked)
        return;

    if (!ui->secondStreamGroupBox->isEnabled())
        return;

    ui->secondStreamDisableCheckBox->setChecked(!value);
}

bool LegacyExpertSettingsWidget::isReadOnly() const
{
    return m_readOnly;
}

void LegacyExpertSettingsWidget::setReadOnly(bool readOnly)
{
    if (m_readOnly == readOnly)
        return;

    using ::setReadOnly;
    setReadOnly(ui->leftWidget, readOnly);
    setReadOnly(ui->rightWidget, readOnly);
    setReadOnly(ui->restoreDefaultsButton, readOnly);

    m_readOnly = readOnly;
}

} // namespace nx::vms::client::desktop
