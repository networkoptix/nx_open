#include "fisheye_settings_widget.h"
#include "ui_fisheye_settings_widget.h"

#include <camera/single_thumbnail_loader.h>

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

void QnFisheyeSettingsWidget::updateFromResource(const QnResourcePtr &resource) {
    QnMediaResourcePtr media = resource.dynamicCast<QnMediaResource>();
    if (!media)
        return;
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    QnMediaDewarpingParams params = media->getDewarpingParams();
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

    ui->calibrateWidget->setRadius(params.radius);
    ui->calibrateWidget->setCenter(QPointF(params.xCenter, params.yCenter));
    ui->calibrateWidget->setImageProvider(QnSingleThumbnailLoader::newInstance(resource, -1, QSize(), ui->calibrateWidget));
}

void QnFisheyeSettingsWidget::submitToResource(const QnResourcePtr &resource) {
    QnMediaResourcePtr media = resource.dynamicCast<QnMediaResource>();
    if (!media)
        return;

    QnMediaDewarpingParams result = media->getDewarpingParams();
    result.fovRot = ui->horizontalSlider->value() / 10.0;
    if (ui->horizontalRadioButton->isChecked())
        result.viewMode = QnMediaDewarpingParams::Horizontal;
    else if (ui->viewDownButton->isChecked())
        result.viewMode = QnMediaDewarpingParams::VerticalDown;
    else
        result.viewMode = QnMediaDewarpingParams::VerticalUp;
    result.xCenter = ui->calibrateWidget->center().x();
    result.yCenter = ui->calibrateWidget->center().y();
    result.radius = ui->calibrateWidget->radius();
    media->setDewarpingParams(result);
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
