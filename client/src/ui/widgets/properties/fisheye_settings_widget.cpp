#include "fisheye_settings_widget.h"
#include "ui_fisheye_settings_widget.h"

#include <core/resource/resource.h>
#include <core/resource/media_resource.h>

#include <ui/widgets/fisheye/fisheye_calibration_widget.h>

#include <utils/common/scoped_value_rollback.h>

QnFisheyeSettingsWidget::QnFisheyeSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::FisheyeSettingsWidget),
    m_updating(false)
{
    ui->setupUi(this);

    connect(ui->angleSpinBox,           SIGNAL(valueChanged(double)),   this, SLOT(updateSliderFromSpinbox(double)));
    connect(ui->horizontalSlider,       SIGNAL(valueChanged(int)),      this, SLOT(updateSpinboxFromSlider(int)));

    connect(ui->angleSpinBox,           SIGNAL(valueChanged(double)),   this, SLOT(at_dataChanged()));
    connect(ui->horizontalSlider,       SIGNAL(valueChanged(int)),      this, SLOT(at_dataChanged()));

    connect(ui->horizontalRadioButton,  SIGNAL(clicked(bool)),          this, SLOT(at_dataChanged()));
    connect(ui->viewDownButton,         SIGNAL(clicked(bool)),          this, SLOT(at_dataChanged()));
    connect(ui->viewUpButton,           SIGNAL(clicked(bool)),          this, SLOT(at_dataChanged()));

    connect(ui->calibrateWidget,        &QnFisheyeCalibrationWidget::dataChanged,   this, &QnFisheyeSettingsWidget::at_dataChanged);
}

QnFisheyeSettingsWidget::~QnFisheyeSettingsWidget() {
}

void QnFisheyeSettingsWidget::updateFromParams(const QnMediaDewarpingParams &params, QnImageProvider *imageProvider) {
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    switch (params.viewMode) {
        case QnMediaDewarpingParams::Horizontal:
            ui->horizontalRadioButton->setChecked(true);
            break;
        case QnMediaDewarpingParams::VerticalUp:
            ui->viewUpButton->setChecked(true);
            break;
        case QnMediaDewarpingParams::VerticalDown:
            ui->viewDownButton->setChecked(true);
            break;
        default:
            Q_ASSERT_X( __LINE__, Q_FUNC_INFO, "Unsupported value");
    }

    ui->angleSpinBox->setValue(params.fovRot);
    ui->horizontalSlider->setValue(params.fovRot * 10);

    if (ui->calibrateWidget->imageProvider() != imageProvider)
    {
        ui->calibrateWidget->init();
        ui->calibrateWidget->setImageProvider(imageProvider);
    }
    ui->calibrateWidget->setRadius(params.radius);
    ui->calibrateWidget->setHorizontalStretch(params.hStretch);
    ui->calibrateWidget->setCenter(QPointF(params.xCenter, params.yCenter));
}

void QnFisheyeSettingsWidget::submitToParams(QnMediaDewarpingParams &params) {
    params.fovRot = ui->horizontalSlider->value() / 10.0;
    if (ui->horizontalRadioButton->isChecked())
        params.viewMode = QnMediaDewarpingParams::Horizontal;
    else if (ui->viewDownButton->isChecked())
        params.viewMode = QnMediaDewarpingParams::VerticalDown;
    else
        params.viewMode = QnMediaDewarpingParams::VerticalUp;
    params.xCenter = ui->calibrateWidget->center().x();
    params.yCenter = ui->calibrateWidget->center().y();
    params.radius = ui->calibrateWidget->radius();
    params.hStretch = ui->calibrateWidget->horizontalStretch();
}

void QnFisheyeSettingsWidget::updateSliderFromSpinbox(double value) {
    int newValue = 10 * value;
    if (ui->horizontalSlider->value() == newValue)
        return;
    ui->horizontalSlider->setValue(newValue);
}

void QnFisheyeSettingsWidget::updateSpinboxFromSlider(int value) {
    int newValue = 0.1 * value;
    if (qFuzzyCompare(ui->angleSpinBox->value(), newValue))
        return;
    ui->angleSpinBox->setValue(newValue);
}

void QnFisheyeSettingsWidget::at_dataChanged() {
    if (m_updating)
        return;
    emit dataChanged();
}

void QnFisheyeSettingsWidget::loadPreview() {
    ui->calibrateWidget->updateImage();
}
