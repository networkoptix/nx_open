#include "expert_settings_widget.h"
#include "ui_expert_settings_widget.h"

#include <ui/style/skin.h>
#include <ui/style/custom_style.h>

#include <common/common_module.h>

#include <client/client_globals.h>

#include <api/global_settings.h>
#include <core/resource/resource_type.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_resource.h>

#include <utils/common/scoped_value_rollback.h>

#include <nx/client/desktop/ui/common/checkbox_utils.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

using namespace nx::client::desktop::ui;

QnCameraExpertSettingsWidget::QnCameraExpertSettingsWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::CameraExpertSettingsWidget),
    m_updating(false),
    m_qualityEditable(false)
{
    ui->setupUi(this);

    NX_ASSERT(parent);
    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(window());
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());
    scrollBar->setUseMaximumSpace(true);

    CheckboxUtils::autoClearTristate(ui->checkBoxForceMotionDetection);

    setWarningStyle(ui->settingsWarningLabel, style::Hints::kDisabledItemOpacity);
    setWarningStyle(ui->settingsDisabledWarningLabel, style::Hints::kDisabledItemOpacity);
    setWarningStyle(ui->highQualityWarningLabel, style::Hints::kDisabledItemOpacity);
    setWarningStyle(ui->lowQualityWarningLabel, style::Hints::kDisabledItemOpacity);
    setWarningStyle(ui->generalWarningLabel, style::Hints::kDisabledItemOpacity);

    ui->iconLabel->setPixmap(qnSkin->pixmap("legacy/warning.png"));
    ui->iconLabel->setScaledContents(true);

    // If "I have read manual" is set, all controls should be enabled.
    connect(ui->assureCheckBox, &QCheckBox::toggled, ui->assureCheckBox, &QWidget::setDisabled);
    connect(ui->assureCheckBox, &QCheckBox::toggled, ui->restoreDefaultsButton, &QWidget::setEnabled);
    connect(ui->assureCheckBox, &QCheckBox::toggled, ui->scrollArea, &QWidget::setEnabled);
    ui->restoreDefaultsButton->setEnabled(false);

    connect(ui->settingsDisableControlCheckBox, &QCheckBox::toggled, ui->qualityGroupBox, &QGroupBox::setDisabled);
    connect(ui->settingsDisableControlCheckBox, &QCheckBox::toggled, this, &QnCameraExpertSettingsWidget::updateControlBlock);
    connect(qnGlobalSettings, &QnGlobalSettings::cameraSettingsOptimizationChanged, this, &QnCameraExpertSettingsWidget::updateControlBlock);
    updateControlBlock();

    connect(ui->qualityOverrideCheckBox, &QCheckBox::toggled, ui->qualitySlider, &QWidget::setVisible);
    connect(ui->qualityOverrideCheckBox, &QCheckBox::toggled, ui->qualityLabelsWidget, &QWidget::setVisible);
    ui->qualitySlider->setVisible(false);
    ui->qualityLabelsWidget->setVisible(false);

    connect(ui->qualitySlider, SIGNAL(valueChanged(int)), this, SLOT(at_qualitySlider_valueChanged(int)));
    ui->highQualityWarningLabel->setVisible(false);
    ui->lowQualityWarningLabel->setVisible(false);

    ui->verticalSpacerLabel->setMinimumHeight(ui->verticalSpacerLabel->fontMetrics().height());
    ui->verticalSpacerLabel_2->setMinimumHeight(ui->verticalSpacerLabel_2->fontMetrics().height());

    connect(ui->restoreDefaultsButton, SIGNAL(clicked()), this, SLOT(at_restoreDefaultsButton_clicked()));

    connect(ui->settingsDisableControlCheckBox, SIGNAL(stateChanged(int)), this, SLOT(at_dataChanged()));
    connect(ui->qualityOverrideCheckBox, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));
    connect(ui->qualitySlider, SIGNAL(valueChanged(int)), this, SLOT(at_dataChanged()));
    connect(ui->checkBoxPrimaryRecorder, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));
    connect(ui->checkBoxBitratePerGOP, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));
    connect(ui->checkBoxSecondaryRecorder, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));
    connect(ui->comboBoxTransport, SIGNAL(currentIndexChanged(int)), this, SLOT(at_dataChanged()));
    connect(ui->checkBoxDisableNativePtzPresets, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));

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
        this, &QnCameraExpertSettingsWidget::at_dataChanged);

    connect(
        ui->comboBoxForcedMotionStream, QnComboboxCurrentIndexChanged,
        this, &QnCameraExpertSettingsWidget::at_dataChanged);

    setHelpTopic(ui->qualityGroupBox, Qn::CameraSettings_SecondStream_Help);
    setHelpTopic(ui->settingsDisableControlCheckBox, Qn::CameraSettings_Expert_SettingsControl_Help);
    setHelpTopic(ui->checkBoxPrimaryRecorder, Qn::CameraSettings_Expert_DisableArchivePrimary_Help);
    setHelpTopic(ui->checkBoxSecondaryRecorder, Qn::CameraSettings_Expert_DisableArchivePrimary_Help);
    setHelpTopic(ui->groupBoxRTP, Qn::CameraSettings_Expert_Rtp_Help);
}

QnCameraExpertSettingsWidget::~QnCameraExpertSettingsWidget()
{

}


void QnCameraExpertSettingsWidget::updateFromResources(const QnVirtualCameraResourceList &cameras)
{
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    bool sameQuality = true;
    bool sameControlState = true;

    Qn::SecondStreamQuality quality = Qn::SSQualityNotDefined;
    bool controlDisabled = false;

    int arecontCamerasCount = 0;
    bool anyHasDualStreaming = false;

    bool isFirstQuality = true;
    bool isFirstControl = true;

    int primaryRecorderDisabled = -1;
    int bitratePerGopPresent = -1;
    int secondaryRecorderDisabled = -1;
    bool samePrimaryRec = true;
    bool sameBitratePerGOP = true;
    bool enableBitratePerGop = true;
    bool sameSecRec = true;

    bool sameRtpTransport = true;
    QString rtpTransport;

    bool sameMdPolicies = true;
    QString mdPolicy;

    const int kPrimaryStreamMdIndex = 0;
    const int kSecondaryStreamMdIndex = 1;
    int forcedMotionStreamIndex = -1;
    bool allCamerasSupportForceMotion = true;

    int supportedNativePtzCount = 0;
    int disabledNativePtzCount = 0;

    int camCnt = 0;
    foreach(const QnVirtualCameraResourcePtr &camera, cameras)
    {
        if (isArecontCamera(camera))
            arecontCamerasCount++;
        if (!camera->supportedMotionType().testFlag(Qn::MT_SoftwareGrid))
            allCamerasSupportForceMotion = false;

        anyHasDualStreaming |= camera->hasDualStreaming();

        if (camera->hasDualStreaming()) {
            if (isFirstQuality) {
                isFirstQuality = false;
                quality = camera->secondaryStreamQuality();
            } else {
                sameQuality &= camera->secondaryStreamQuality() == quality;
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

        Qn::BitratePerGopType bpgType = camera->bitratePerGopType();
        if (bpgType == Qn::BPG_Predefined)
            enableBitratePerGop = false;
        int bitratePerGop = bpgType == Qn::BPG_None ? 0 : 1;
        if (bitratePerGopPresent == -1)
            bitratePerGopPresent = bitratePerGop;
        else if (bitratePerGopPresent != bitratePerGop)
            sameBitratePerGOP = false;

        QString camRtpTransport = camera->getProperty(QnMediaResource::rtpTransportKey());
        if (camRtpTransport != rtpTransport && camCnt > 0)
            sameRtpTransport = false;
        rtpTransport = camRtpTransport;

        auto forcedMotionStreamProperty = camera->getProperty(QnMediaResource::motionStreamKey());
        if (forcedMotionStreamProperty != mdPolicy && camCnt > 0)
            sameMdPolicies = false;
        mdPolicy = forcedMotionStreamProperty;

        if (!forcedMotionStreamProperty.isEmpty())
        {
            if (forcedMotionStreamProperty == QnMediaResource::primaryStreamValue())
            {
                forcedMotionStreamIndex = kPrimaryStreamMdIndex;
            }
            else if (forcedMotionStreamProperty == QnMediaResource::secondaryStreamValue()
                && forcedMotionStreamIndex != kPrimaryStreamMdIndex)
            {
                forcedMotionStreamIndex = kSecondaryStreamMdIndex;
            }
        }

        if (camera->getPtzCapabilities() & Ptz::NativePresetsPtzCapability)
            ++supportedNativePtzCount;

        if (!camera->getProperty(Qn::DISABLE_NATIVE_PTZ_PRESETS_PARAM_NAME).isEmpty())
            ++disabledNativePtzCount;

        camCnt++;
    }

    m_qualityEditable = anyHasDualStreaming;
    ui->qualityGroupBox->setVisible(anyHasDualStreaming);
    if (sameQuality && quality != Qn::SSQualityNotDefined) {
        ui->qualityOverrideCheckBox->setChecked(true);
        ui->qualityOverrideCheckBox->setVisible(false);
        ui->qualitySlider->setValue(qualityToSliderPos(quality));
    }
    else {
        ui->qualityOverrideCheckBox->setChecked(false);
        ui->qualityOverrideCheckBox->setVisible(true);
        ui->qualitySlider->setValue(qualityToSliderPos(Qn::SSQualityMedium));
    }

    if (samePrimaryRec)
        ui->checkBoxPrimaryRecorder->setChecked(primaryRecorderDisabled);
    else
        ui->checkBoxPrimaryRecorder->setCheckState(Qt::PartiallyChecked);

    if (sameBitratePerGOP)
        ui->checkBoxBitratePerGOP->setChecked(bitratePerGopPresent);
    else
        ui->checkBoxBitratePerGOP->setCheckState(Qt::PartiallyChecked);
    ui->checkBoxBitratePerGOP->setEnabled(enableBitratePerGop);

    ui->checkBoxSecondaryRecorder->setEnabled(anyHasDualStreaming);
    if (anyHasDualStreaming) {
        if (sameSecRec)
            ui->checkBoxSecondaryRecorder->setChecked(secondaryRecorderDisabled);
        else
            ui->checkBoxSecondaryRecorder->setCheckState(Qt::PartiallyChecked);
    }
    else {
        ui->checkBoxSecondaryRecorder->setChecked(false);
    }

    if (rtpTransport.isEmpty())
        ui->comboBoxTransport->setCurrentIndex(0);
    else if (sameRtpTransport)
        ui->comboBoxTransport->setCurrentText(rtpTransport);
    else
        ui->comboBoxTransport->setCurrentIndex(-1);

    ui->comboBoxForcedMotionStream->clear();
    ui->comboBoxForcedMotionStream->addItem(tr("Primary"), QnMediaResource::primaryStreamValue());

    if (anyHasDualStreaming)
        ui->comboBoxForcedMotionStream->addItem(tr("Secondary"), QnMediaResource::secondaryStreamValue());

    bool gotForcedMotionStream = forcedMotionStreamIndex != -1;

    CheckboxUtils::setupTristateCheckbox(
        ui->checkBoxForceMotionDetection,
        sameMdPolicies,
        gotForcedMotionStream);

    ui->comboBoxForcedMotionStream->setCurrentIndex(
         gotForcedMotionStream ? forcedMotionStreamIndex : kPrimaryStreamMdIndex);

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

    ui->groupBoxPtzControl->setEnabled(supportedNativePtzCount != 0);
    if (supportedNativePtzCount != 0 && disabledNativePtzCount == supportedNativePtzCount)
        ui->checkBoxDisableNativePtzPresets->setCheckState(Qt::Checked);
    else if (disabledNativePtzCount != 0)
        ui->checkBoxDisableNativePtzPresets->setCheckState(Qt::PartiallyChecked);
    else
        ui->checkBoxDisableNativePtzPresets->setCheckState(Qt::Unchecked);

    bool defaultValues = ui->settingsDisableControlCheckBox->checkState() == Qt::Unchecked
            && sliderPosToQuality(ui->qualitySlider->value()) == Qn::SSQualityMedium
            && ui->checkBoxPrimaryRecorder->checkState() == Qt::Unchecked
            && (ui->checkBoxBitratePerGOP->checkState() == Qt::Unchecked || !enableBitratePerGop)
            && ui->checkBoxSecondaryRecorder->checkState() == Qt::Unchecked
            && ui->comboBoxTransport->currentIndex() == 0
            && ui->checkBoxForceMotionDetection->checkState() == Qt::Unchecked
            && ui->checkBoxDisableNativePtzPresets->checkState() == Qt::Unchecked;

    ui->assureCheckBox->setEnabled(!cameras.isEmpty() && defaultValues);
    ui->assureCheckBox->setChecked(!defaultValues);
    ui->scrollArea->setEnabled(ui->assureCheckBox->isChecked());

}

void QnCameraExpertSettingsWidget::submitToResources(const QnVirtualCameraResourceList &cameras) {
    if (!ui->assureCheckBox->isChecked())
        return;

    bool disableControls = ui->settingsDisableControlCheckBox->checkState() == Qt::Checked;
    bool enableControls = ui->settingsDisableControlCheckBox->checkState() == Qt::Unchecked;
    bool globalControlEnabled = qnGlobalSettings->isCameraSettingsOptimizationEnabled();
    auto disableNativePtz = ui->checkBoxDisableNativePtzPresets->checkState();

    Qn::SecondStreamQuality quality = (Qn::SecondStreamQuality) sliderPosToQuality(ui->qualitySlider->value());

    for (const QnVirtualCameraResourcePtr &camera: cameras)
    {
        if (globalControlEnabled && !isArecontCamera(camera))
        {
            if (disableControls)
                camera->setCameraControlDisabled(true);
            else if (enableControls)
                camera->setCameraControlDisabled(false);
        }

        if (globalControlEnabled && enableControls && ui->qualityOverrideCheckBox->isChecked() && camera->hasDualStreaming())
            camera->setSecondaryStreamQuality(quality);

        if (ui->checkBoxPrimaryRecorder->checkState() != Qt::PartiallyChecked)
            camera->setProperty(QnMediaResource::dontRecordPrimaryStreamKey(), ui->checkBoxPrimaryRecorder->isChecked() ? lit("1") : lit("0"));
        if (ui->checkBoxBitratePerGOP->checkState() != Qt::PartiallyChecked)
            camera->setProperty(Qn::FORCE_BITRATE_PER_GOP, ui->checkBoxBitratePerGOP->isChecked() ? lit("1") : lit("0"));
        if (ui->checkBoxSecondaryRecorder->checkState() != Qt::PartiallyChecked && camera->hasDualStreaming())
            camera->setProperty(QnMediaResource::dontRecordSecondaryStreamKey(), ui->checkBoxSecondaryRecorder->isChecked() ? lit("1") : lit("0"));

        if (ui->comboBoxTransport->currentIndex() >= 0) {
            QString txt = ui->comboBoxTransport->currentText();
            if (txt.toLower() == lit("auto"))
                txt.clear();
            camera->setProperty(QnMediaResource::rtpTransportKey(), txt);
        }

        auto mdPolicyCheckState = ui->checkBoxForceMotionDetection->checkState();
        if (mdPolicyCheckState == Qt::Unchecked)
        {
            camera->setProperty(QnMediaResource::motionStreamKey(), QString());
        }
        else if (mdPolicyCheckState == Qt::Checked)
        {
            auto index = ui->comboBoxForcedMotionStream->currentIndex();
            if (index >= 0)
            {
                auto mdPolicy = ui->comboBoxForcedMotionStream->itemData(index).toString();

                if (isMdPolicyAllowedForCamera(camera, mdPolicy))
                    camera->setProperty(QnMediaResource::motionStreamKey(), mdPolicy);
            }
        }

        if (disableNativePtz != Qt::PartiallyChecked
            && (camera->getPtzCapabilities() & Ptz::NativePresetsPtzCapability))
        {
                camera->setProperty(
                    Qn::DISABLE_NATIVE_PTZ_PRESETS_PARAM_NAME,
                    (disableNativePtz == Qt::Checked) ? lit("true") : QString());
        }
    }
}


bool QnCameraExpertSettingsWidget::isArecontCamera(const QnVirtualCameraResourcePtr &camera) const {
    QnResourceTypePtr cameraType = qnResTypePool->getResourceType(camera->getTypeId());
    return cameraType && cameraType->getManufacture() == lit("ArecontVision");
}

bool QnCameraExpertSettingsWidget::isMdPolicyAllowedForCamera(const QnVirtualCameraResourcePtr& camera, const QString& mdPolicy) const
{
    bool hasDualStreaming  = camera->hasDualStreaming();

    return mdPolicy.isEmpty() //< Do not force MD policy
        || mdPolicy == QnMediaResource::primaryStreamValue()
        || (mdPolicy == QnMediaResource::secondaryStreamValue() && hasDualStreaming);
}

void QnCameraExpertSettingsWidget::at_dataChanged()
{
    if (!m_updating)
        emit dataChanged();
}

void QnCameraExpertSettingsWidget::at_restoreDefaultsButton_clicked()
{
    ui->settingsDisableControlCheckBox->setCheckState(Qt::Unchecked);
    ui->qualityOverrideCheckBox->setChecked(true);
    ui->qualitySlider->setValue(qualityToSliderPos(Qn::SSQualityMedium));
    ui->checkBoxPrimaryRecorder->setChecked(false);
    ui->checkBoxBitratePerGOP->setChecked(false);
    ui->checkBoxSecondaryRecorder->setChecked(false);
    ui->comboBoxTransport->setCurrentIndex(0);
    ui->checkBoxForceMotionDetection->setCheckState(Qt::Unchecked);
    ui->comboBoxForcedMotionStream->setCurrentIndex(0);
    ui->checkBoxDisableNativePtzPresets->setCheckState(Qt::Unchecked);
}

void QnCameraExpertSettingsWidget::at_qualitySlider_valueChanged(int value) {
    Qn::SecondStreamQuality quality = sliderPosToQuality(value);
    ui->lowQualityWarningLabel->setVisible(quality == Qn::SSQualityLow);
    ui->highQualityWarningLabel->setVisible(quality == Qn::SSQualityHigh);
}

void QnCameraExpertSettingsWidget::updateControlBlock() {
    bool globalControlEnabled = qnGlobalSettings->isCameraSettingsOptimizationEnabled();
    ui->settingsDisableControlCheckBox->setEnabled(globalControlEnabled);
    ui->settingsDisabledWarningLabel->setVisible(!globalControlEnabled);
    ui->settingsWarningLabel->setVisible(globalControlEnabled && !ui->settingsDisableControlCheckBox->isChecked());
}

Qn::SecondStreamQuality QnCameraExpertSettingsWidget::sliderPosToQuality(int pos) const {
    if (pos == 0)
        return Qn::SSQualityDontUse;
    else
        return Qn::SecondStreamQuality(pos-1);
}

int QnCameraExpertSettingsWidget::qualityToSliderPos(Qn::SecondStreamQuality quality)
{
    if (quality == Qn::SSQualityDontUse)
        return 0;
    else
        return int(quality) + 1;
}

bool QnCameraExpertSettingsWidget::isSecondStreamEnabled() const {
    if (!m_qualityEditable)
        return true;

    if (ui->settingsDisableControlCheckBox->checkState() != Qt::Unchecked)
        return true;

    if (!ui->qualityGroupBox->isEnabled())
        return true;

    if (!ui->qualityOverrideCheckBox->isChecked())
        return true;

    Qn::SecondStreamQuality quality = (Qn::SecondStreamQuality) sliderPosToQuality(ui->qualitySlider->value());
    return quality != Qn::SSQualityDontUse;
}

void QnCameraExpertSettingsWidget::setSecondStreamEnabled(bool value /* = true*/) {
    if (!m_qualityEditable)
        return;

    if (ui->settingsDisableControlCheckBox->checkState() != Qt::Unchecked)
        return;

    if (!ui->qualityGroupBox->isEnabled())
        return;

    if (value) {
        ui->qualitySlider->setValue(qualityToSliderPos(Qn::SSQualityMedium));
    } else {
        ui->qualitySlider->setValue(qualityToSliderPos(Qn::SSQualityDontUse));
    }
}
