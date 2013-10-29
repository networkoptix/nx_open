#include "expert_settings_widget.h"
#include "ui_expert_settings_widget.h"

#include <ui/style/skin.h>
#include <ui/style/warning_style.h>

#include <core/resource/resource_type.h>
#include <core/resource/camera_resource.h>

#include <utils/common/scoped_value_rollback.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

QnAdvancedSettingsWidget::QnAdvancedSettingsWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::AdvancedSettingsWidget),
    m_updating(false)
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

    connect(ui->restoreDefaultsButton, SIGNAL(clicked()), this, SLOT(at_restoreDefaultsButton_clicked()));

    connect(ui->settingsDisableControlCheckBox, SIGNAL(stateChanged(int)), this, SLOT(at_dataChanged()));
    connect(ui->qualityOverrideCheckBox, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));
    connect(ui->qualitySlider, SIGNAL(valueChanged(int)), this, SLOT(at_dataChanged()));

    setHelpTopic(ui->qualityGroupBox, Qn::CameraSettings_SecondStream_Help);
}

QnAdvancedSettingsWidget::~QnAdvancedSettingsWidget()
{

}


void QnAdvancedSettingsWidget::updateFromResources(const QnVirtualCameraResourceList &cameras)
{
    QnScopedValueRollback<bool> guard(&m_updating, true); Q_UNUSED(guard)

    bool sameQuality = true;
    bool sameControlState = true;

    Qn::SecondStreamQuality quality = Qn::SSQualityNotDefined;
    bool controlDisabled = false;

    int arecontCamerasCount = 0;
    bool anyHasDualStreaming = false;

    bool isFirstQuality = true;
    bool isFirstControl = true;


    foreach(const QnVirtualCameraResourcePtr &camera, cameras) {
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
    }

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

    ui->settingsGroupBox->setVisible(arecontCamerasCount != cameras.size());
    ui->settingsDisableControlCheckBox->setTristate(!sameControlState);
    if (sameControlState)
        ui->settingsDisableControlCheckBox->setChecked(controlDisabled);
    else
        ui->settingsDisableControlCheckBox->setCheckState(Qt::PartiallyChecked);

    bool defaultValues = ui->settingsDisableControlCheckBox->checkState() == Qt::Unchecked
            && sliderPosToQuality(ui->qualitySlider->value()) == Qn::SSQualityMedium;

    ui->assureCheckBox->setEnabled(!cameras.isEmpty() && defaultValues);
    ui->assureCheckBox->setChecked(!defaultValues);

    if (arecontCamerasCount != cameras.size() || anyHasDualStreaming)
        ui->stackedWidget->setCurrentWidget(ui->pageSettings);
    else
        ui->stackedWidget->setCurrentWidget(ui->pageNoSettings);
}

void QnAdvancedSettingsWidget::submitToResources(const QnVirtualCameraResourceList &cameras) {
    if (!ui->assureCheckBox->isChecked())
        return;

    bool disableControls = ui->settingsDisableControlCheckBox->checkState() == Qt::Checked;
    bool enableControls = ui->settingsDisableControlCheckBox->checkState() == Qt::Unchecked;

    Qn::SecondStreamQuality quality = (Qn::SecondStreamQuality) sliderPosToQuality(ui->qualitySlider->value());

    foreach(const QnVirtualCameraResourcePtr &camera, cameras) {
        if (!isArecontCamera(camera)) {
            if (disableControls)
                camera->setCameraControlDisabled(true);
            else if (enableControls)
                camera->setCameraControlDisabled(false);
        }

        if (enableControls && ui->qualityOverrideCheckBox->isChecked() && camera->hasDualStreaming())
            camera->setSecondaryStreamQuality(quality);
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
}

void QnAdvancedSettingsWidget::at_qualitySlider_valueChanged(int value) {
    Qn::SecondStreamQuality quality = sliderPosToQuality(value);
    ui->lowQualityWarningLabel->setVisible(quality == Qn::SSQualityLow);
    ui->highQualityWarningLabel->setVisible(quality == Qn::SSQualityHigh);
}

Qn::SecondStreamQuality QnAdvancedSettingsWidget::sliderPosToQuality(int pos)
{
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
