#include "expert_settings_widget.h"
#include "ui_expert_settings_widget.h"

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

#include <utils/common/scoped_value_rollback.h>

#include <nx/client/desktop/ui/common/checkbox_utils.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

using namespace nx::client::desktop::ui;

namespace {

// Native ptz presets can be disabled if they are supported and nx presets are supported too.
bool canDisableNativePresets(const QnVirtualCameraResourcePtr& camera)
{
    const auto caps = camera->getPtzCapabilities();
    return caps.testFlag(Ptz::NativePresetsPtzCapability)
        && !caps.testFlag(Ptz::NoNxPresetsPtzCapability);
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

CameraExpertSettingsWidget::CameraExpertSettingsWidget(QWidget* parent):
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
    setWarningStyle(ui->generalWarningLabel);

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
        this, &CameraExpertSettingsWidget::updateControlBlock);
    connect(ui->bitratePerGopCheckBox, &QCheckBox::toggled,
        this, &CameraExpertSettingsWidget::updateControlBlock);
    connect(qnGlobalSettings, &QnGlobalSettings::cameraSettingsOptimizationChanged,
        this, &CameraExpertSettingsWidget::updateControlBlock);
    updateControlBlock();

    connect(ui->restoreDefaultsButton, SIGNAL(clicked()), this, SLOT(at_restoreDefaultsButton_clicked()));

    connect(ui->settingsDisableControlCheckBox, SIGNAL(stateChanged(int)), this, SLOT(at_dataChanged()));
    connect(ui->checkBoxPrimaryRecorder, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));
    connect(ui->bitratePerGopCheckBox, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));
    connect(ui->checkBoxSecondaryRecorder, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));
    connect(ui->comboBoxTransport, SIGNAL(currentIndexChanged(int)), this, SLOT(at_dataChanged()));
    connect(ui->checkBoxDisableNativePtzPresets, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));

    connect(ui->secondStreamDisableCheckBox, &QCheckBox::stateChanged,
        this, &CameraExpertSettingsWidget::at_dataChanged);

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
        this, &CameraExpertSettingsWidget::at_dataChanged);

    connect(
        ui->comboBoxForcedMotionStream, QnComboboxCurrentIndexChanged,
        this, &CameraExpertSettingsWidget::at_dataChanged);

    setHelpTopic(ui->secondStreamGroupBox, Qn::CameraSettings_SecondStream_Help);
    setHelpTopic(ui->settingsDisableControlCheckBox, Qn::CameraSettings_Expert_SettingsControl_Help);
    setHelpTopic(ui->checkBoxPrimaryRecorder, Qn::CameraSettings_Expert_DisableArchivePrimary_Help);
    setHelpTopic(ui->checkBoxSecondaryRecorder, Qn::CameraSettings_Expert_DisableArchivePrimary_Help);
    setHelpTopic(ui->groupBoxRTP, Qn::CameraSettings_Expert_Rtp_Help);
}

CameraExpertSettingsWidget::~CameraExpertSettingsWidget()
{

}

void CameraExpertSettingsWidget::updateFromResources(const QnVirtualCameraResourceList &cameras)
{
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

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
    bool enableBitratePerGop = true;
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

    int supportedNativePtzCount = 0;
    int disabledNativePtzCount = 0;

    int camCnt = 0;
    for (const auto& camera: cameras)
    {
        if (isArecontCamera(camera))
            arecontCamerasCount++;
        if (!camera->supportedMotionType().testFlag(Qn::MT_SoftwareGrid))
            allCamerasSupportForceMotion = false;

        m_hasDualStreaming |= camera->hasDualStreaming();
        m_hasRemoteArchiveCapability |= camera->hasCameraCapabilities(Qn::RemoteArchiveCapability);

        if (camera->hasDualStreaming())
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
            else if(forcedMotionStreamProperty == QnMediaResource::edgeStreamValue()
                && forcedMotionStreamIndex != kPrimaryStreamMdIndex
                && forcedMotionStreamIndex != kSecondaryStreamMdIndex)
            {
                forcedMotionStreamIndex = kEdgeStreamMdIndex;
            }
        }

        if (canDisableNativePresets(camera))
        {
            ++supportedNativePtzCount;
            if (!camera->getProperty(Qn::DISABLE_NATIVE_PTZ_PRESETS_PARAM_NAME).isEmpty())
                ++disabledNativePtzCount;
        }

        camCnt++;
    }

    ui->secondStreamGroupBox->setVisible(m_hasDualStreaming);

    if (m_hasDualStreaming)
    {
        CheckboxUtils::setupTristateCheckbox(
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

    if (m_hasRemoteArchiveCapability)
        ui->comboBoxForcedMotionStream->addItem(tr("Edge"), QnMediaResource::edgeStreamValue());

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

void CameraExpertSettingsWidget::submitToResources(const QnVirtualCameraResourceList& cameras)
{
    bool disableControls = ui->settingsDisableControlCheckBox->checkState() == Qt::Checked;
    bool enableControls = ui->settingsDisableControlCheckBox->checkState() == Qt::Unchecked;
    bool globalControlEnabled = qnGlobalSettings->isCameraSettingsOptimizationEnabled();
    auto disableNativePtz = ui->checkBoxDisableNativePtzPresets->checkState();

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
            if (ui->secondStreamDisableCheckBox->checkState() != Qt::PartiallyChecked)
                camera->setDisableDualStreaming(
                    ui->secondStreamDisableCheckBox->checkState() == Qt::Checked);
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

        if (disableNativePtz != Qt::PartiallyChecked && canDisableNativePresets(camera))
        {
            camera->setProperty(
                Qn::DISABLE_NATIVE_PTZ_PRESETS_PARAM_NAME,
                (disableNativePtz == Qt::Checked) ? lit("true") : QString());
        }
    }
}


bool CameraExpertSettingsWidget::isArecontCamera(const QnVirtualCameraResourcePtr &camera) const {
    QnResourceTypePtr cameraType = qnResTypePool->getResourceType(camera->getTypeId());
    return cameraType && cameraType->getManufacture() == lit("ArecontVision");
}

bool CameraExpertSettingsWidget::isMdPolicyAllowedForCamera(const QnVirtualCameraResourcePtr& camera, const QString& mdPolicy) const
{
    const bool hasDualStreaming  = camera->hasDualStreaming();
    const bool hasRemoteArchive = camera->hasCameraCapabilities(Qn::RemoteArchiveCapability);

    return mdPolicy.isEmpty() //< Do not force MD policy
        || mdPolicy == QnMediaResource::primaryStreamValue()
        || (mdPolicy == QnMediaResource::secondaryStreamValue() && hasDualStreaming)
        || (mdPolicy == QnMediaResource::edgeStreamValue() && hasRemoteArchive);
}

void CameraExpertSettingsWidget::at_dataChanged()
{
    if (!m_updating)
        emit dataChanged();
}

bool CameraExpertSettingsWidget::areDefaultValues() const
{
    if (ui->bitratePerGopCheckBox->isEnabled() && ui->bitratePerGopCheckBox->isChecked())
        return false;

    return ui->settingsDisableControlCheckBox->checkState() == Qt::Unchecked
        && ui->checkBoxPrimaryRecorder->checkState() == Qt::Unchecked
        && ui->checkBoxSecondaryRecorder->checkState() == Qt::Unchecked
        && ui->comboBoxTransport->currentIndex() == 0
        && ui->checkBoxForceMotionDetection->checkState() == Qt::Unchecked
        && ui->checkBoxDisableNativePtzPresets->checkState() == Qt::Unchecked
        && ui->secondStreamDisableCheckBox->checkState() == Qt::Unchecked;
}

void CameraExpertSettingsWidget::at_restoreDefaultsButton_clicked()
{
    ui->settingsDisableControlCheckBox->setCheckState(Qt::Unchecked);
    ui->secondStreamDisableCheckBox->setChecked(false);
    ui->checkBoxPrimaryRecorder->setChecked(false);
    ui->bitratePerGopCheckBox->setChecked(false);
    ui->checkBoxSecondaryRecorder->setChecked(false);
    ui->comboBoxTransport->setCurrentIndex(0);
    ui->checkBoxForceMotionDetection->setCheckState(Qt::Unchecked);
    ui->comboBoxForcedMotionStream->setCurrentIndex(0);
    ui->checkBoxDisableNativePtzPresets->setCheckState(Qt::Unchecked);
}

void CameraExpertSettingsWidget::updateControlBlock()
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

bool CameraExpertSettingsWidget::isSecondStreamEnabled() const
{
    if (ui->settingsDisableControlCheckBox->checkState() != Qt::Unchecked)
        return true;

    if (!ui->secondStreamGroupBox->isEnabled())
        return true;

    return ui->secondStreamDisableCheckBox->checkState() == Qt::Unchecked;
}

void CameraExpertSettingsWidget::setSecondStreamEnabled(bool value)
{
    if (ui->settingsDisableControlCheckBox->checkState() != Qt::Unchecked)
        return;

    if (!ui->secondStreamGroupBox->isEnabled())
        return;

    ui->secondStreamDisableCheckBox->setChecked(!value);
}

} // namespace desktop
} // namespace client
} // namespace nx
