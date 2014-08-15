#include "expert_settings_widget.h"
#include "ui_expert_settings_widget.h"

#include <ui/style/skin.h>
#include <ui/style/warning_style.h>

#include <client/client_globals.h>

#include <core/resource/resource_type.h>
#include <core/resource/camera_resource.h>

#include <utils/common/scoped_value_rollback.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

QnAdvancedSettingsWidget::QnAdvancedSettingsWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::AdvancedSettingsWidget),
    m_updating(false),
    m_qualityEditable(false)
{
    ui->setupUi(this);

    setWarningStyle(ui->settingsWarningLabel);
    setWarningStyle(ui->highQualityWarningLabel);
    setWarningStyle(ui->lowQualityWarningLabel);
    setWarningStyle(ui->generalWarningLabel);

    // if "I have read manual" is set, all controls should be enabled
    connect(ui->assureCheckBox, SIGNAL(toggled(bool)), ui->assureCheckBox, SLOT(setDisabled(bool)));
    connect(ui->assureCheckBox, SIGNAL(toggled(bool)), ui->assureWidget, SLOT(setEnabled(bool)));
    ui->assureWidget->setEnabled(false);

    connect(ui->settingsDisableControlCheckBox, SIGNAL(toggled(bool)), ui->qualityGroupBox, SLOT(setDisabled(bool)));
    connect(ui->settingsDisableControlCheckBox, SIGNAL(toggled(bool)), ui->settingsWarningLabel, SLOT(setVisible(bool)));
    ui->settingsWarningLabel->setVisible(false);

    connect(ui->qualityOverrideCheckBox, SIGNAL(toggled(bool)), ui->qualitySlider, SLOT(setVisible(bool)));
    connect(ui->qualityOverrideCheckBox, SIGNAL(toggled(bool)), ui->qualityLabelsWidget, SLOT(setVisible(bool)));
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
    connect(ui->checkBoxSecondaryRecorder, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));
    connect(ui->comboBoxTransport, SIGNAL(currentIndexChanged(int)), this, SLOT(at_dataChanged()));

    setHelpTopic(ui->qualityGroupBox, Qn::CameraSettings_SecondStream_Help);
    setHelpTopic(ui->settingsDisableControlCheckBox, Qn::CameraSettings_Expert_SettingsControl_Help);
}

QnAdvancedSettingsWidget::~QnAdvancedSettingsWidget()
{

}


void QnAdvancedSettingsWidget::updateFromResources(const QnVirtualCameraResourceList &cameras)
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
    int secondaryRecorderDisabled = -1;
    bool samePrimaryRec = true;
    bool sameSecRec = true;

    bool sameRtpTransport = true;
    QString rtpTransport;

    bool sameMotionStream = true;
    QString motionStream;

    int camCnt = 0;
    foreach(const QnVirtualCameraResourcePtr &camera, cameras) 
    {
        if (isArecontCamera(camera))
            arecontCamerasCount++;
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

        QString camRtpTransport = camera->getProperty(QnMediaResource::rtpTransportKey());
        if (camRtpTransport != rtpTransport && camCnt > 0)
            sameRtpTransport = false;
        rtpTransport = camRtpTransport;

        QString camMotionStream = camera->getProperty(QnMediaResource::motionStreamKey());
        if (camMotionStream != motionStream && camCnt > 0)
            sameMotionStream = false;
        motionStream = camMotionStream;

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

    ui->settingsGroupBox->setVisible(arecontCamerasCount != cameras.size());
    ui->settingsDisableControlCheckBox->setTristate(!sameControlState);
    if (sameControlState)
        ui->settingsDisableControlCheckBox->setChecked(controlDisabled);
    else
        ui->settingsDisableControlCheckBox->setCheckState(Qt::PartiallyChecked);

    bool defaultValues = ui->settingsDisableControlCheckBox->checkState() == Qt::Unchecked
            && sliderPosToQuality(ui->qualitySlider->value()) == Qn::SSQualityMedium
            && ui->checkBoxPrimaryRecorder->checkState() == Qt::Unchecked
            && ui->checkBoxSecondaryRecorder->checkState() == Qt::Unchecked
            && ui->comboBoxTransport->currentIndex() == 0;

    ui->assureCheckBox->setEnabled(!cameras.isEmpty() && defaultValues);
    ui->assureCheckBox->setChecked(!defaultValues);
}

void QnAdvancedSettingsWidget::submitToResources(const QnVirtualCameraResourceList &cameras) {
    if (!ui->assureCheckBox->isChecked())
        return;

    bool disableControls = ui->settingsDisableControlCheckBox->checkState() == Qt::Checked;
    bool enableControls = ui->settingsDisableControlCheckBox->checkState() == Qt::Unchecked;

    Qn::SecondStreamQuality quality = (Qn::SecondStreamQuality) sliderPosToQuality(ui->qualitySlider->value());

    foreach(const QnVirtualCameraResourcePtr &camera, cameras) 
    {
        if (!isArecontCamera(camera)) {
            if (disableControls)
                camera->setCameraControlDisabled(true);
            else if (enableControls)
                camera->setCameraControlDisabled(false);
        }

        if (enableControls && ui->qualityOverrideCheckBox->isChecked() && camera->hasDualStreaming())
            camera->setSecondaryStreamQuality(quality);

        if (ui->checkBoxPrimaryRecorder->checkState() != Qt::PartiallyChecked)
            camera->setProperty(QnMediaResource::dontRecordPrimaryStreamKey(), ui->checkBoxPrimaryRecorder->isChecked() ? lit("1") : lit("0"));
        if (ui->checkBoxSecondaryRecorder->checkState() != Qt::PartiallyChecked && camera->hasDualStreaming())
            camera->setProperty(QnMediaResource::dontRecordSecondaryStreamKey(), ui->checkBoxSecondaryRecorder->isChecked() ? lit("1") : lit("0"));

        if (ui->comboBoxTransport->currentIndex() >= 0) {
            QString txt = ui->comboBoxTransport->currentText();
            if (txt.toLower() == lit("auto"))
                txt.clear();
            camera->setProperty(QnMediaResource::rtpTransportKey(), txt);
        }

    }
}


bool QnAdvancedSettingsWidget::isArecontCamera(const QnVirtualCameraResourcePtr &camera) const {
    QnResourceTypePtr cameraType = qnResTypePool->getResourceType(camera->getTypeId());
    return cameraType && cameraType->getManufacture() == lit("ArecontVision");
}

void QnAdvancedSettingsWidget::at_dataChanged()
{
    if (!m_updating)
        emit dataChanged();
}

void QnAdvancedSettingsWidget::at_restoreDefaultsButton_clicked()
{
    ui->settingsDisableControlCheckBox->setCheckState(Qt::Unchecked);
    ui->qualityOverrideCheckBox->setChecked(true);
    ui->qualitySlider->setValue(qualityToSliderPos(Qn::SSQualityMedium));
    ui->checkBoxPrimaryRecorder->setChecked(false);
    ui->checkBoxSecondaryRecorder->setChecked(false);
    ui->comboBoxTransport->setCurrentIndex(0);
}

void QnAdvancedSettingsWidget::at_qualitySlider_valueChanged(int value) {
    Qn::SecondStreamQuality quality = sliderPosToQuality(value);
    ui->lowQualityWarningLabel->setVisible(quality == Qn::SSQualityLow);
    ui->highQualityWarningLabel->setVisible(quality == Qn::SSQualityHigh);
}

Qn::SecondStreamQuality QnAdvancedSettingsWidget::sliderPosToQuality(int pos) const {
    if (pos == 0)
        return Qn::SSQualityDontUse;
    else
        return Qn::SecondStreamQuality(pos-1);
}

int QnAdvancedSettingsWidget::qualityToSliderPos(Qn::SecondStreamQuality quality)
{
    if (quality == Qn::SSQualityDontUse)
        return 0;
    else
        return int(quality) + 1;
}

bool QnAdvancedSettingsWidget::isSecondStreamEnabled() const {
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

void QnAdvancedSettingsWidget::setSecondStreamEnabled(bool value /*= true*/) {
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
