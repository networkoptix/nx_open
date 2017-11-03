#include "fisheye_calibration_widget.h"
#include "ui_fisheye_calibration_widget.h"

#include <QtCore/QTimer>
#include <QtGui/QPainter>

#include <client/client_globals.h>

#include <ui/fisheye/fisheye_calibrator.h>
#include <ui/widgets/fisheye/fisheye_calibration_image_widget.h>

#include <utils/image_provider.h>
#include <nx/utils/math/fuzzy.h>

QnFisheyeCalibrationWidget::QnFisheyeCalibrationWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnFisheyeCalibrationWidget),
    m_calibrator(new QnFisheyeCalibrator()),
    m_lastError(QnFisheyeCalibrator::NoError)
{
    ui->setupUi(this);
    ui->loadingWidget->setText(tr("Loading preview, please wait..."));

    connect(m_calibrator,       &QnFisheyeCalibrator::centerChanged,    ui->imageWidget,    &QnFisheyeCalibrationImageWidget::setCenter);
    connect(m_calibrator,       &QnFisheyeCalibrator::radiusChanged,    ui->imageWidget,    &QnFisheyeCalibrationImageWidget::setRadius);
    connect(m_calibrator,       &QnFisheyeCalibrator::stretchChanged,   ui->imageWidget,    &QnFisheyeCalibrationImageWidget::setStretch);
    connect(m_calibrator,       &QnFisheyeCalibrator::finished,         this,               &QnFisheyeCalibrationWidget::at_calibrator_finished);

    connect(m_calibrator,       &QnFisheyeCalibrator::centerChanged,    this,               &QnFisheyeCalibrationWidget::dataChanged);
    connect(m_calibrator,       &QnFisheyeCalibrator::radiusChanged,    this,               &QnFisheyeCalibrationWidget::dataChanged);
    connect(m_calibrator,       &QnFisheyeCalibrator::stretchChanged,   this,               &QnFisheyeCalibrationWidget::dataChanged);

    connect(ui->imageWidget,    &QnFisheyeCalibrationImageWidget::centerModified, this,     &QnFisheyeCalibrationWidget::setCenter);
    connect(ui->imageWidget,    &QnFisheyeCalibrationImageWidget::radiusModified, this,     &QnFisheyeCalibrationWidget::setRadius);
    connect(ui->imageWidget,    &QnFisheyeCalibrationImageWidget::animationFinished, this,  &QnFisheyeCalibrationWidget::at_image_animationFinished);

    init();
}

QnFisheyeCalibrationWidget::~QnFisheyeCalibrationWidget()
{
}

void QnFisheyeCalibrationWidget::init()
{
    ui->imageWidget->abortSearchAnimation();
    ui->imageWidget->setImage(QImage());
    setImageProvider(nullptr);
    updatePage();
}

QnImageProvider* QnFisheyeCalibrationWidget::imageProvider() const
{
    return m_imageProvider.data();
}

// TODO: #GDM change to QnCameraThumbnailManager
void QnFisheyeCalibrationWidget::setImageProvider(QnImageProvider* provider)
{
    if (m_imageProvider)
    {
        ui->imageWidget->disconnect(m_imageProvider);
        m_imageProvider->disconnect(this);
    }

    m_imageProvider = provider;

    if (!m_imageProvider)
        return;

    connect(m_imageProvider, &QnImageProvider::imageChanged, ui->imageWidget,
        &QnFisheyeCalibrationImageWidget::setImage);
    connect(m_imageProvider, &QnImageProvider::statusChanged, this,
        &QnFisheyeCalibrationWidget::updatePage);
    connect(m_imageProvider, &QnImageProvider::imageChanged, this,
        &QnFisheyeCalibrationWidget::updatePage);

    updatePage();
}

QPointF QnFisheyeCalibrationWidget::center() const
{
    return m_calibrator->center();
}

void QnFisheyeCalibrationWidget::setCenter(const QPointF &center)
{
    m_calibrator->setCenter(center);
    update();
}

void QnFisheyeCalibrationWidget::setHorizontalStretch(const qreal &value)
{
    m_calibrator->setHorizontalStretch(value);
    update();
}

qreal QnFisheyeCalibrationWidget::horizontalStretch() const
{
    return m_calibrator->horizontalStretch();
}

qreal QnFisheyeCalibrationWidget::radius() const
{
    return m_calibrator->radius();
}

void QnFisheyeCalibrationWidget::setRadius(qreal radius)
{
    m_calibrator->setRadius(radius);
    update();
}

void QnFisheyeCalibrationWidget::updatePage()
{
    const bool imageLoaded = m_imageProvider &&
        (m_imageProvider->status() == Qn::ThumbnailStatus::Loaded
            || m_imageProvider->status() == Qn::ThumbnailStatus::Refreshing);

    if (imageLoaded)
        ui->imageWidget->setImage(m_imageProvider->image());
    ui->stackedWidget->setCurrentWidget(imageLoaded
        ? ui->imagePage
        : ui->loadingPage);
}

void QnFisheyeCalibrationWidget::at_calibrator_finished(int errorCode)
{
    m_lastError = errorCode;
    ui->imageWidget->endSearchAnimation();
}

void QnFisheyeCalibrationWidget::at_image_animationFinished()
{
    emit autoCalibrationFinished();
    switch (m_lastError)
    {
        case QnFisheyeCalibrator::ErrorNotFisheyeImage:
            QnMessageBox::critical(this,
                tr("Auto calibration failed"), tr("Image is not round."));
            break;

        case QnFisheyeCalibrator::ErrorTooLowLight:
            QnMessageBox::critical(this,
                tr("Auto calibration failed"), tr("Image might be too dim."));
            break;

        default:
            break;
    }
}

void QnFisheyeCalibrationWidget::autoCalibrate()
{
    ui->imageWidget->beginSearchAnimation();
    m_calibrator->analyseFrameAsync(ui->imageWidget->image());
}
