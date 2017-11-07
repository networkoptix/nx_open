#include "expert_settings_widget.h"
#include "ui_expert_settings_widget.h"

#include <QtWidgets/QListView>

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

namespace {

bool secondStreamDisabled(const QnVirtualCameraResourcePtr& camera)
{
    return camera->secondaryStreamQuality() == Qn::SSQualityDontUse;
}

struct SecondStreamMode
{
    bool disabled = false;
    Qn::SecondStreamQuality quality = Qn::SSQualityMedium;

    SecondStreamMode() = default;
    SecondStreamMode(Qn::SecondStreamQuality storedQuality)
    {
        disabled = storedQuality == Qn::SSQualityDontUse;
        quality = disabled ? Qn::SSQualityMedium : storedQuality;
    }

    Qn::SecondStreamQuality storedQuality() const
    {
        return disabled ? Qn::SSQualityDontUse : quality;
    }
};

} // namespace

QnCameraExpertSettingsWidget::QnCameraExpertSettingsWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::CameraExpertSettingsWidget)
{
    ui->setupUi(this);

    NX_ASSERT(parent);
    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(window());
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());
    scrollBar->setUseMaximumSpace(true);

    CheckboxUtils::autoClearTristate(ui->checkBoxForceMotionDetection);
    CheckboxUtils::autoClearTristate(ui->secondStreamDisableCheckBox);

    setWarningStyle(ui->settingsWarningLabel);
    setWarningStyle(ui->bitrateIncreaseWarningLabel);
    setWarningStyle(ui->highQualityWarningLabel);
    setWarningStyle(ui->lowQualityWarningLabel);
    setWarningStyle(ui->generalWarningLabel);

    ui->settingsWarningLabel->setVisible(false);
    ui->bitrateIncreaseWarningLabel->setVisible(false);
    ui->highQualityWarningLabel->setVisible(false);
    ui->lowQualityWarningLabel->setVisible(false);

    ui->secondStreamQualityComboBox->addItem(tr("Don't change"), Qn::SSQualityNotDefined);
    ui->secondStreamQualityComboBox->addItem(tr("Low"), Qn::SSQualityLow);
    ui->secondStreamQualityComboBox->addItem(tr("Medium"), Qn::SSQualityMedium);
    ui->secondStreamQualityComboBox->addItem(tr("High"), Qn::SSQualityHigh);

    ui->iconLabel->setPixmap(qnSkin->pixmap("legacy/warning.png"));
    ui->iconLabel->setScaledContents(true);

    connect(ui->settingsDisableControlCheckBox, &QCheckBox::toggled,
        ui->secondStreamGroupBox, &QGroupBox::setDisabled);
    connect(ui->settingsDisableControlCheckBox, &QCheckBox::toggled,
        this, &QnCameraExpertSettingsWidget::updateControlBlock);
    connect(ui->bitratePerGopCheckBox, &QCheckBox::toggled,
        this, &QnCameraExpertSettingsWidget::updateControlBlock);
    connect(qnGlobalSettings, &QnGlobalSettings::cameraSettingsOptimizationChanged,
        this, &QnCameraExpertSettingsWidget::updateControlBlock);
    updateControlBlock();

    connect(ui->restoreDefaultsButton, SIGNAL(clicked()), this, SLOT(at_restoreDefaultsButton_clicked()));

    connect(ui->settingsDisableControlCheckBox, SIGNAL(stateChanged(int)), this, SLOT(at_dataChanged()));
    connect(ui->checkBoxPrimaryRecorder, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));
    connect(ui->bitratePerGopCheckBox, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));
    connect(ui->checkBoxSecondaryRecorder, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));
    connect(ui->comboBoxTransport, SIGNAL(currentIndexChanged(int)), this, SLOT(at_dataChanged()));
    connect(ui->checkBoxDisableNativePtzPresets, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));

    connect(ui->secondStreamDisableCheckBox, &QCheckBox::stateChanged,
        this, &QnCameraExpertSettingsWidget::at_secondStreamQualityChanged);
    connect(ui->secondStreamQualityComboBox, QnComboboxCurrentIndexChanged,
        this, &QnCameraExpertSettingsWidget::at_secondStreamQualityChanged);

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

    setHelpTopic(ui->secondStreamGroupBox, Qn::CameraSettings_SecondStream_Help);
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
    bool sameSecondStreamDisabledState = true;
    SecondStreamMode secondStreamMode;

    bool sameControlState = true;
    bool controlDisabled = false;

    int arecontCamerasCount = 0;
    m_hasDualStreaming = false;

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

    m_qualityEditable = true;

    const int kIndexDontChange = ui->secondStreamQualityComboBox->findData(Qn::SSQualityNotDefined);

    int camCnt = 0;
    for (const auto& camera: cameras)
    {
        if (isArecontCamera(camera))
            arecontCamerasCount++;
        if (!camera->supportedMotionType().testFlag(Qn::MT_SoftwareGrid))
            allCamerasSupportForceMotion = false;

        const bool qualityEditable = camera->cameraMediaCapability().isNull();
        m_hasDualStreaming |= camera->hasDualStreaming();

        m_qualityEditable &= qualityEditable;

        if (camera->hasDualStreaming())
        {
            const SecondStreamMode currentMode(camera->secondaryStreamQuality());
            if (isFirstQuality)
            {
                isFirstQuality = false;
                secondStreamMode = currentMode;
            }
            else
            {
                sameQuality &= currentMode.quality == secondStreamMode.quality;
                sameSecondStreamDisabledState &= currentMode.disabled == secondStreamMode.disabled;
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

    m_qualityEditable &= m_hasDualStreaming;
    ui->secondStreamGroupBox->setVisible(m_hasDualStreaming);

    if (m_hasDualStreaming)
    {
        CheckboxUtils::setupTristateCheckbox(
            ui->secondStreamDisableCheckBox,
            sameSecondStreamDisabledState,
            secondStreamMode.disabled);

        ui->secondStreamQualityWidget->setVisible(m_qualityEditable
            && sameSecondStreamDisabledState && !secondStreamMode.disabled);

        static_cast<QListView*>(ui->secondStreamQualityComboBox->view())->setRowHidden(
            kIndexDontChange,
            cameras.size() == 1);

        ui->secondStreamQualityComboBox->setCurrentIndex(sameQuality
            ? ui->secondStreamQualityComboBox->findData(secondStreamMode.quality)
            : kIndexDontChange);
    }

    if (samePrimaryRec)
        ui->checkBoxPrimaryRecorder->setChecked(primaryRecorderDisabled);
    else
        ui->checkBoxPrimaryRecorder->setCheckState(Qt::PartiallyChecked);

    if (sameBitratePerGOP)
        ui->bitratePerGopCheckBox->setChecked(bitratePerGopPresent);
    else
        ui->bitratePerGopCheckBox->setCheckState(Qt::PartiallyChecked);

    ui->bitratePerGopCheckBox->setEnabled(enableBitratePerGop);

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

    ui->comboBoxForcedMotionStream->clear();
    ui->comboBoxForcedMotionStream->addItem(tr("Primary"), QnMediaResource::primaryStreamValue());

    if (m_hasDualStreaming)
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
}

Qn::SecondStreamQuality QnCameraExpertSettingsWidget::selectedSecondStreamQuality() const
{
    switch (ui->secondStreamDisableCheckBox->checkState())
    {
        case Qt::Checked:
            return Qn::SSQualityDontUse;

        case Qt::PartiallyChecked:
            return Qn::SSQualityNotDefined;

        case Qt::Unchecked:
            return m_qualityEditable
                ? Qn::SecondStreamQuality(ui->secondStreamQualityComboBox->currentData().toInt())
                : Qn::SSQualityMedium;

        default:
            NX_EXPECT(false);
            return Qn::SSQualityNotDefined;
    }
}

void QnCameraExpertSettingsWidget::setSelectedSecondStreamQuality(Qn::SecondStreamQuality value) const
{
    ui->secondStreamDisableCheckBox->setChecked(value == Qn::SSQualityDontUse);

    if (value != Qn::SSQualityDontUse)
    {
        ui->secondStreamQualityComboBox->setCurrentIndex(
            ui->secondStreamQualityComboBox->findData(value));
    }
}

void QnCameraExpertSettingsWidget::submitToResources(const QnVirtualCameraResourceList& cameras)
{
    bool disableControls = ui->settingsDisableControlCheckBox->checkState() == Qt::Checked;
    bool enableControls = ui->settingsDisableControlCheckBox->checkState() == Qt::Unchecked;
    bool globalControlEnabled = qnGlobalSettings->isCameraSettingsOptimizationEnabled();
    auto disableNativePtz = ui->checkBoxDisableNativePtzPresets->checkState();

    const auto quality = selectedSecondStreamQuality();
    for (const auto& camera: cameras)
    {
        if (globalControlEnabled && !isArecontCamera(camera))
        {
            if (disableControls)
                camera->setCameraControlDisabled(true);
            else if (enableControls)
                camera->setCameraControlDisabled(false);
        }

        if (globalControlEnabled && enableControls && camera->hasDualStreaming())
        {
            if (m_qualityEditable) //< All cameras are old-style.
            {
                // If quality was explicitly selected.
                if (quality != Qn::SSQualityNotDefined)
                    camera->setSecondaryStreamQuality(quality);
            }
            else //< Some or all cameras are new-style.
            {
                if (camera->cameraMediaCapability().isNull())
                {
                    // If old-style camera second stream was either enabled or disabled.
                    if (quality == Qn::SSQualityDontUse
                        || (secondStreamDisabled(camera) && quality != Qn::SSQualityNotDefined))
                    {
                        camera->setSecondaryStreamQuality(quality);
                    }
                }
                else
                {
                    // If new-style camera second stream was enabled or disabled.
                    if (quality != Qn::SSQualityNotDefined)
                        camera->setSecondaryStreamQuality(quality);
                }
            }
        }

        if (ui->checkBoxPrimaryRecorder->checkState() != Qt::PartiallyChecked)
            camera->setProperty(QnMediaResource::dontRecordPrimaryStreamKey(), ui->checkBoxPrimaryRecorder->isChecked() ? lit("1") : lit("0"));
        if (ui->bitratePerGopCheckBox->checkState() != Qt::PartiallyChecked)
            camera->setProperty(Qn::FORCE_BITRATE_PER_GOP, ui->bitratePerGopCheckBox->isChecked() ? lit("1") : lit("0"));
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

bool QnCameraExpertSettingsWidget::areDefaultValues() const
{
    if (m_hasDualStreaming && selectedSecondStreamQuality() != Qn::SSQualityMedium)
        return false;

    if (ui->bitratePerGopCheckBox->isEnabled() && ui->bitratePerGopCheckBox->isChecked())
        return false;

    return ui->settingsDisableControlCheckBox->checkState() == Qt::Unchecked
        && ui->checkBoxPrimaryRecorder->checkState() == Qt::Unchecked
        && ui->checkBoxSecondaryRecorder->checkState() == Qt::Unchecked
        && ui->comboBoxTransport->currentIndex() == 0
        && ui->checkBoxForceMotionDetection->checkState() == Qt::Unchecked
        && ui->checkBoxDisableNativePtzPresets->checkState() == Qt::Unchecked;
}

void QnCameraExpertSettingsWidget::at_restoreDefaultsButton_clicked()
{
    ui->settingsDisableControlCheckBox->setCheckState(Qt::Unchecked);
    if (m_hasDualStreaming)
        setSelectedSecondStreamQuality(Qn::SSQualityMedium);
    ui->checkBoxPrimaryRecorder->setChecked(false);
    ui->bitratePerGopCheckBox->setChecked(false);
    ui->checkBoxSecondaryRecorder->setChecked(false);
    ui->comboBoxTransport->setCurrentIndex(0);
    ui->checkBoxForceMotionDetection->setCheckState(Qt::Unchecked);
    ui->comboBoxForcedMotionStream->setCurrentIndex(0);
    ui->checkBoxDisableNativePtzPresets->setCheckState(Qt::Unchecked);
}

void QnCameraExpertSettingsWidget::at_secondStreamQualityChanged()
{
    const auto quality = selectedSecondStreamQuality();
    ui->lowQualityWarningLabel->setVisible(quality == Qn::SSQualityLow);
    ui->highQualityWarningLabel->setVisible(quality == Qn::SSQualityHigh);
    ui->secondStreamQualityWidget->setVisible(quality != Qn::SSQualityDontUse && m_qualityEditable);
    ui->secondStreamGroupBox->layout()->activate();
    ui->leftWidget->layout()->activate();

    at_dataChanged();
}

void QnCameraExpertSettingsWidget::updateControlBlock()
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

bool QnCameraExpertSettingsWidget::isSecondStreamEnabled() const
{
    if (!m_qualityEditable)
        return true;

    if (ui->settingsDisableControlCheckBox->checkState() != Qt::Unchecked)
        return true;

    if (!ui->secondStreamGroupBox->isEnabled())
        return true;

    return ui->secondStreamDisableCheckBox->checkState() == Qt::Unchecked;
}

void QnCameraExpertSettingsWidget::setSecondStreamEnabled(bool value)
{
    if (!m_qualityEditable)
        return;

    if (ui->settingsDisableControlCheckBox->checkState() != Qt::Unchecked)
        return;

    if (!ui->secondStreamGroupBox->isEnabled())
        return;

    ui->secondStreamDisableCheckBox->setChecked(!value);
}
