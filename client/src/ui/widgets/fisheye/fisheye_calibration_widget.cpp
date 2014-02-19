#include "fisheye_calibration_widget.h"
#include "ui_fisheye_calibration_widget.h"

#include <QtCore/QTimer>
#include <QtGui/QPainter>
#include <QtWidgets/QMessageBox>

#include <ui/fisheye/fisheye_calibrator.h>
#include <ui/widgets/fisheye/fisheye_calibration_image_widget.h>

#include <utils/image_provider.h>
#include <utils/math/fuzzy.h>
#include <utils/common/scoped_painter_rollback.h>

namespace {
    int refreshInterval = 5000; //update image every interval (in ms)
}

QnFisheyeCalibrationWidget::QnFisheyeCalibrationWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnFisheyeCalibrationWidget),
    m_calibrator(new QnFisheyeCalibrator()),
    m_imageProvider(NULL),
    m_updating(false),
    m_lastError(QnFisheyeCalibrator::NoError)
{
    ui->setupUi(this);
    
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(refreshInterval);
    connect(m_updateTimer,      &QTimer::timeout,                       this,               &QnFisheyeCalibrationWidget::at_updateTimer_timeout);

    connect(m_calibrator,       &QnFisheyeCalibrator::centerChanged,    ui->imageWidget,    &QnFisheyeCalibrationImageWidget::setCenter);
    connect(m_calibrator,       &QnFisheyeCalibrator::radiusChanged,    ui->imageWidget,    &QnFisheyeCalibrationImageWidget::setRadius);
    connect(m_calibrator,       &QnFisheyeCalibrator::finished,         this,               &QnFisheyeCalibrationWidget::at_calibrator_finished);

    connect(ui->autoButton,     &QPushButton::clicked,                  this,               &QnFisheyeCalibrationWidget::at_autoButton_clicked);

    connect(ui->xCenterSlider,  &QSlider::valueChanged,                 this,               &QnFisheyeCalibrationWidget::at_xCenterSlider_valueChanged);
    connect(ui->yCenterSlider,  &QSlider::valueChanged,                 this,               &QnFisheyeCalibrationWidget::at_yCenterSlider_valueChanged);
    connect(ui->radiusSlider,   &QSlider::valueChanged,                 this,               &QnFisheyeCalibrationWidget::at_radiusSlider_valueChanged);
    connect(m_calibrator,       &QnFisheyeCalibrator::centerChanged,    this,               &QnFisheyeCalibrationWidget::at_calibrator_centerChanged);
    connect(m_calibrator,       &QnFisheyeCalibrator::radiusChanged,    this,               &QnFisheyeCalibrationWidget::at_calibrator_radiusChanged);

    connect(m_calibrator,       &QnFisheyeCalibrator::centerChanged,    this,               &QnFisheyeCalibrationWidget::dataChanged);
    connect(m_calibrator,       &QnFisheyeCalibrator::radiusChanged,    this,               &QnFisheyeCalibrationWidget::dataChanged);

    connect(ui->imageWidget,    &QnFisheyeCalibrationImageWidget::centerModified, this,     &QnFisheyeCalibrationWidget::setCenter);
    connect(ui->imageWidget,    &QnFisheyeCalibrationImageWidget::radiusModified, this,     &QnFisheyeCalibrationWidget::setRadius);
    connect(ui->imageWidget,    &QnFisheyeCalibrationImageWidget::animationFinished, this,  &QnFisheyeCalibrationWidget::at_image_animationFinished);

    init();
}

QnFisheyeCalibrationWidget::~QnFisheyeCalibrationWidget() {}

void QnFisheyeCalibrationWidget::init() {
    ui->imageWidget->abortSearchAnimation();
    ui->imageWidget->setImage(QImage());
    setImageProvider(NULL);

    at_imageProvider_imageChanged();
}

void QnFisheyeCalibrationWidget::setImageProvider(QnImageProvider *provider) {
    // TODO: #GDM ownership is not clear. Does this object claim ownership of provider?
    // If not, then it should not rely on destruction order => need to store provider in 
    // QPointer and check that it's alive before usage.

    if (m_imageProvider) {
        disconnect(m_updateTimer, NULL, m_imageProvider, NULL);
        disconnect(m_imageProvider, NULL, ui->imageWidget, NULL);
        disconnect(m_imageProvider, NULL, this, NULL);
    }

    m_updateTimer->stop();
    m_imageProvider = provider;

    if (!m_imageProvider)
        return;

    connect(m_imageProvider, &QnImageProvider::imageChanged, ui->imageWidget,   &QnFisheyeCalibrationImageWidget::setImage);
    connect(m_imageProvider, &QnImageProvider::imageChanged, this,              &QnFisheyeCalibrationWidget::at_imageProvider_imageChanged);

    m_updateTimer->start();

    if (!m_imageProvider->image().isNull())
        ui->imageWidget->setImage(provider->image());
    m_imageProvider->loadAsync();
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

void QnFisheyeCalibrationWidget::at_imageProvider_imageChanged() {
    bool imageLoaded = m_imageProvider && !m_imageProvider->image().isNull();
    ui->autoButton->setVisible(imageLoaded);
    ui->xCenterSlider->setVisible(imageLoaded);
    ui->yCenterSlider->setVisible(imageLoaded);
    ui->radiusSlider->setVisible(imageLoaded);
}

void QnFisheyeCalibrationWidget::at_calibrator_finished(int errorCode) {
    m_lastError = errorCode;
    ui->imageWidget->endSearchAnimation();
}

void QnFisheyeCalibrationWidget::at_image_animationFinished() {
    ui->autoButton->setEnabled(true);
    switch (m_lastError) {
    case QnFisheyeCalibrator::ErrorNotFisheyeImage:
        QMessageBox::warning(this, tr("Error"), tr("Autodetection failed. Image is not round."));
        break;
    case QnFisheyeCalibrator::ErrorTooLowLight:
        QMessageBox::warning(this, tr("Error"), tr("Autodetection failed. The image might be too dim."));
        break;
    default:
        break;
    }
}

void QnFisheyeCalibrationWidget::at_autoButton_clicked() {
    ui->imageWidget->beginSearchAnimation();
    ui->autoButton->setEnabled(false);
    m_calibrator->analyseFrameAsync(ui->imageWidget->image());
}

void QnFisheyeCalibrationWidget::at_xCenterSlider_valueChanged(int value) {
    if (m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
    setCenter(QPointF(0.01 * value, center().y()));
}

void QnFisheyeCalibrationWidget::at_yCenterSlider_valueChanged(int value) {
    if (m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
    setCenter(QPointF(center().x(), 1.0 - 0.01 * value));
}

void QnFisheyeCalibrationWidget::at_radiusSlider_valueChanged(int value) {
    if (m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
    setRadius(0.01 * value);
}

void QnFisheyeCalibrationWidget::at_calibrator_centerChanged(const QPointF &center) {
    if (m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
    ui->xCenterSlider->setValue(center.x() * 100);
    ui->yCenterSlider->setValue((1.0 - center.y()) * 100);
}

void QnFisheyeCalibrationWidget::at_calibrator_radiusChanged(qreal radius) {
    if (m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
    ui->radiusSlider->setValue(radius * 100);
}

void QnFisheyeCalibrationWidget::at_updateTimer_timeout() {
    if(m_imageProvider && isVisible())
        m_imageProvider->loadAsync();
}