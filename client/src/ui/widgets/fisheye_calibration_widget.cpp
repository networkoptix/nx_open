#include "fisheye_calibration_widget.h"
#include "ui_fisheye_calibration_widget.h"

#include <QtGui/QPainter>

#include <ui/fisheye/fisheye_calibrator.h>

#include <utils/math/fuzzy.h>
#include <utils/common/scoped_painter_rollback.h>

QnFisheyeCalibrationWidget::QnFisheyeCalibrationWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnFisheyeCalibrationWidget),
    m_calibrator(new QnFisheyeCalibrator())
{
    ui->setupUi(this);
    ui->imageLabel->addPaintDelegate(this);

//    connect(m_calibrator, &QnFisheyeCalibrator::centerChanged,  ui->calibrateWidget,    &QnFisheyeCalibrateWidget::setCenter);
//    connect(m_calibrator, &QnFisheyeCalibrator::radiusChanged,  ui->calibrateWidget,    &QnFisheyeCalibrateWidget::setRadius);
    connect(m_calibrator, &QnFisheyeCalibrator::finished,       this,   &QnFisheyeCalibrationWidget::at_calibrator_finished);

    connect(ui->startButton, &QPushButton::clicked, this, &QnFisheyeCalibrationWidget::at_startButton_clicked);
}

QnFisheyeCalibrationWidget::~QnFisheyeCalibrationWidget() {}

void QnFisheyeCalibrationWidget::delegatedPaint(const QWidget *widget, QPainter *painter) {
    if (widget != ui->imageLabel)
       return;

    qreal radius = m_calibrator->radius();

    if (qFuzzyIsNull(radius))
        return;

    bool pixmapExists = !m_image.isNull();
    if (!pixmapExists)
        return;

    int frame = ui->imageLabel->lineWidth() / 2;
    QRect targetRect = widget->rect().adjusted(frame, frame, -frame, -frame);
    QPointF center(m_calibrator->center().x() * targetRect.width(), m_calibrator->center().y() * targetRect.height());
//    center += ui->imageLabel->pos();
    radius *= qMin(targetRect.width(), targetRect.height());

    QPen pen;
    pen.setWidth(4);
    pen.setColor(Qt::red);

    QnScopedPainterPenRollback penRollback(painter, pen);
    painter->setClipRect(targetRect.adjusted(frame, frame, -frame, -frame));
    painter->drawEllipse(center, radius, radius);
    painter->drawEllipse(center, 4, 4);
    painter->setClipping(false);
}

void QnFisheyeCalibrationWidget::setImageProvider(QnImageProvider *provider) {
    if (!provider)
        return;

    connect(provider, &QnImageProvider::imageChanged, this, &QnFisheyeCalibrationWidget::at_imageLoaded);
    at_imageLoaded(provider->image());
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
    ui->imageLabel->setPixmap(QPixmap::fromImage(image.scaled(this->width(), 400, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    update();
}

void QnFisheyeCalibrationWidget::at_calibrator_finished() {
    ui->imageLabel->repaint();
}


void QnFisheyeCalibrationWidget::at_startButton_clicked() {
    m_calibrator->analyseFrameAsync(m_image);
}
