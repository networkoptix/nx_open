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

    ui->settingsWarningLabel->setVisible(false);
    ui->highQualityWarningLabel->setVisible(false);
    ui->lowQualityWarningLabel->setVisible(false);

    ui->settingsDisableControlCheckBox->setEnabled(false);
    ui->qualitySlider->setEnabled(false);
    ui->qualityLabelsWidget->setEnabled(false);
    ui->restoreDefaultsButton->setEnabled(false);

    connect(ui->settingsAssureCheckBox, SIGNAL(toggled(bool)), this, SLOT(at_settingsAssureCheckBox_toggled(bool)));
    connect(ui->settingsDisableControlCheckBox, SIGNAL(toggled(bool)), ui->settingsWarningLabel, SLOT(setVisible(bool)));
    connect(ui->qualityAssureCheckBox, SIGNAL(toggled(bool)), this, SLOT(at_qualityAssureCheckBox_toggled(bool)));
    connect(ui->qualitySlider, SIGNAL(valueChanged(int)), this, SLOT(at_qualitySlider_valueChanged(int)));
    connect(ui->restoreDefaultsButton, SIGNAL(clicked()), this, SLOT(at_restoreDefaultsButton_clicked()));

    connect(ui->settingsAssureCheckBox, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));
    connect(ui->settingsDisableControlCheckBox, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));
    connect(ui->qualityAssureCheckBox, SIGNAL(toggled(bool)), this, SLOT(at_dataChanged()));
    connect(ui->qualitySlider, SIGNAL(valueChanged(int)), this, SLOT(at_dataChanged()));

    setHelpTopic(this, Qn::CameraSettings_Expert_Help);
    setHelpTopic(ui->qualityGroupBox, Qn::CameraSettings_SecondStream_Help);
}

QnAdvancedSettingsWidget::~QnAdvancedSettingsWidget()
{

}


void QnAdvancedSettingsWidget::updateFromResources(const QnVirtualCameraResourceList &cameras)
{
    QnScopedValueRollback<bool> guard(&m_updating, true); Q_UNUSED(guard)

    ui->settingsAssureCheckBox->setEnabled(true);
    ui->settingsAssureCheckBox->setChecked(false);
    ui->qualityAssureCheckBox->setEnabled(true);
    ui->qualityAssureCheckBox->setChecked(false);

    if (cameras.isEmpty()) {
        ui->settingsGroupBox->setEnabled(false);
        ui->qualityGroupBox->setEnabled(false);
        return;
    }

    bool sameQuality = true;
    bool sameControlState = true;

    Qn::SecondStreamQuality quality = Qn::SSQualityNotDefined;
    bool controlDisabled = false;

    int arecontCamerasCount = 0;
    bool anyHasDualStreaming = false;

    bool isFirstCamera = true;


    foreach(const QnVirtualCameraResourcePtr &camera, cameras) {
        if (isArecontCamera(camera))
            arecontCamerasCount++;
        anyHasDualStreaming |= camera->hasDualStreaming();

        if (isFirstCamera) {
            isFirstCamera = false;
            quality = camera->secondaryStreamQuality();
            controlDisabled = camera->isCameraControlDisabled();
            continue;
        }

        if (camera->hasDualStreaming())
            sameQuality &= camera->secondaryStreamQuality() == quality;

        if (!isArecontCamera(camera))
            sameControlState &= camera->isCameraControlDisabled() == controlDisabled;
    }

    ui->qualityGroupBox->setEnabled(anyHasDualStreaming);
    if (ui->qualityGroupBox->isEnabled() && sameQuality && quality != Qn::SSQualityMedium && quality != Qn::SSQualityNotDefined) {
        ui->qualityAssureCheckBox->setChecked(true);
        ui->qualityAssureCheckBox->setEnabled(false);
        ui->qualitySlider->setValue(quality);
    }

    ui->settingsGroupBox->setEnabled(arecontCamerasCount != cameras.size());
    if (ui->settingsGroupBox->isEnabled() && sameControlState && controlDisabled) {
        ui->settingsAssureCheckBox->setChecked(true);
        ui->settingsAssureCheckBox->setEnabled(false);
        ui->settingsDisableControlCheckBox->setChecked(true);
    }


}

void QnAdvancedSettingsWidget::submitToResources(const QnVirtualCameraResourceList &cameras) {
    bool disableControls = ui->settingsDisableControlCheckBox->isChecked();
    Qn::SecondStreamQuality quality = (Qn::SecondStreamQuality) ui->qualitySlider->value();

    foreach(const QnVirtualCameraResourcePtr &camera, cameras) {
        if (ui->settingsAssureCheckBox->isChecked() && !isArecontCamera(camera))
            camera->setCameraControlDisabled(disableControls);
        if (ui->qualityAssureCheckBox->isChecked() && camera->hasDualStreaming())
            camera->setSecondaryStreamQuality(quality);
    }
}


bool QnAdvancedSettingsWidget::isArecontCamera(const QnVirtualCameraResourcePtr &camera) const {
    QnResourceTypePtr cameraType = qnResTypePool->getResourceType(camera->getTypeId());
    return cameraType && cameraType->getManufacture() == lit("ArecontVision");
}


bool QnAdvancedSettingsWidget::isDefaultValues() const {
    return (!ui->settingsDisableControlCheckBox->isChecked() &&
            ui->qualitySlider->value() == Qn::SSQualityMedium);
}

void QnAdvancedSettingsWidget::at_dataChanged()
{
    if (!m_updating)
        emit dataChanged();
    ui->restoreDefaultsButton->setEnabled(!isDefaultValues());
}

void QnAdvancedSettingsWidget::at_restoreDefaultsButton_clicked()
{
    if (ui->settingsAssureCheckBox->isEnabled())
        ui->settingsAssureCheckBox->setChecked(false);
    else
        ui->settingsDisableControlCheckBox->setChecked(false);

    if (ui->qualityAssureCheckBox->isEnabled())
        ui->qualityAssureCheckBox->setChecked(false);
    else
        ui->qualitySlider->setValue(Qn::SSQualityMedium);
}

void QnAdvancedSettingsWidget::at_qualitySlider_valueChanged(int value) {
    ui->lowQualityWarningLabel->setVisible(value == Qn::SSQualityLow);
    ui->highQualityWarningLabel->setVisible(value == Qn::SSQualityHigh);
}

void QnAdvancedSettingsWidget::at_settingsAssureCheckBox_toggled(bool checked) {
    ui->settingsDisableControlCheckBox->setEnabled(checked);
    if (!checked)
        ui->settingsDisableControlCheckBox->setChecked(false);
}

void QnAdvancedSettingsWidget::at_qualityAssureCheckBox_toggled(bool checked) {
    ui->qualitySlider->setEnabled(checked);
    ui->qualityLabelsWidget->setEnabled(checked);
    if (!checked)
        ui->qualitySlider->setValue(Qn::SSQualityMedium);
}
