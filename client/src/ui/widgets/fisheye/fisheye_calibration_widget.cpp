#include "fisheye_calibration_widget.h"
#include "ui_fisheye_calibration_widget.h"

#include <QtGui/QPainter>

#include <ui/fisheye/fisheye_calibrator.h>
#include <ui/widgets/fisheye/fisheye_calibration_image_widget.h>

#include <utils/math/fuzzy.h>
#include <utils/common/scoped_painter_rollback.h>

QnFisheyeCalibrationWidget::QnFisheyeCalibrationWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnFisheyeCalibrationWidget),
    m_calibrator(new QnFisheyeCalibrator())
{
    ui->setupUi(this);

    connect(m_calibrator, &QnFisheyeCalibrator::centerChanged,  ui->imageWidget,    &QnFisheyeCalibrationImageWidget::setCenter);
    connect(m_calibrator, &QnFisheyeCalibrator::radiusChanged,  ui->imageWidget,    &QnFisheyeCalibrationImageWidget::setRadius);
    connect(m_calibrator, &QnFisheyeCalibrator::finished,       this,   &QnFisheyeCalibrationWidget::at_calibrator_finished);

    connect(ui->startButton, &QPushButton::clicked, this, &QnFisheyeCalibrationWidget::at_startButton_clicked);
}

QnFisheyeCalibrationWidget::~QnFisheyeCalibrationWidget() {}

void QnFisheyeCalibrationWidget::setImageProvider(QnImageProvider *provider) {
    if (!provider)
        return;

    connect(provider, &QnImageProvider::imageChanged, this, &QnFisheyeCalibrationWidget::at_imageLoaded);
    at_imageLoaded(provider->image());

    connect(provider, &QnImageProvider::imageChanged, ui->imageWidget,    &QnFisheyeCalibrationImageWidget::setImage);
    ui->imageWidget->setImage(provider->image());

    provider->loadAsync();
}

QPointF QnFisheyeCalibrationWidget::center() const {
    return m_calibrator->center();
}

void QnFisheyeCalibrationWidget::setCenter(const QPointF &center) {
    m_calibrator->setCenter(center);
    update();
}

qreal QnFisheyeCalibrationWidget::radius() const {
    return m_calibrator->radius();
}

void QnFisheyeCalibrationWidget::setRadius(qreal radius) {
    m_calibrator->setRadius(radius);
    update();
}

void QnFisheyeCalibrationWidget::at_imageLoaded(const QImage &image) {
    m_image = image;
}

void QnFisheyeCalibrationWidget::at_calibrator_finished(int errorCode) {
    //TODO: #GDM PTZ handle errorCode
    ui->imageWidget->repaint();
}


void QnFisheyeCalibrationWidget::at_startButton_clicked() {
    m_calibrator->analyseFrameAsync(m_image);
}
