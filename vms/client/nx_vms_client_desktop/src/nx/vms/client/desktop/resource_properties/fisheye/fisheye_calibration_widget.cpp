#include "fisheye_calibration_widget.h"
#include "ui_fisheye_calibration_widget.h"
#include "fisheye_calibration_image_widget.h"

#include <QtCore/QTimer>
#include <QtGui/QPainter>

#include <client/client_globals.h>
#include <ui/common/read_only.h>
#include <ui/fisheye/fisheye_calibrator.h>

#include <nx/vms/client/desktop/image_providers/image_provider.h>
#include <nx/utils/math/fuzzy.h>

namespace nx::vms::client::desktop {

FisheyeCalibrationWidget::FisheyeCalibrationWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::FisheyeCalibrationWidget),
    m_calibrator(new QnFisheyeCalibrator()),
    m_lastError(QnFisheyeCalibrator::NoError)
{
    ui->setupUi(this);
    ui->loadingWidget->setText(tr("Loading preview, please wait..."));
    connect(m_calibrator,       &QnFisheyeCalibrator::centerChanged,    ui->imageWidget,    &FisheyeCalibrationImageWidget::setCenter);
    connect(m_calibrator,       &QnFisheyeCalibrator::radiusChanged,    ui->imageWidget,    &FisheyeCalibrationImageWidget::setRadius);
    connect(m_calibrator,       &QnFisheyeCalibrator::stretchChanged,   ui->imageWidget,    &FisheyeCalibrationImageWidget::setStretch);
    connect(m_calibrator,       &QnFisheyeCalibrator::finished,         this,               &FisheyeCalibrationWidget::at_calibrator_finished);

    connect(m_calibrator,       &QnFisheyeCalibrator::centerChanged,    this,               &FisheyeCalibrationWidget::dataChanged);
    connect(m_calibrator,       &QnFisheyeCalibrator::radiusChanged,    this,               &FisheyeCalibrationWidget::dataChanged);
    connect(m_calibrator,       &QnFisheyeCalibrator::stretchChanged,   this,               &FisheyeCalibrationWidget::dataChanged);

    connect(ui->imageWidget,    &FisheyeCalibrationImageWidget::centerModified, this,     &FisheyeCalibrationWidget::setCenter);
    connect(ui->imageWidget,    &FisheyeCalibrationImageWidget::radiusModified, this,     &FisheyeCalibrationWidget::setRadius);
    connect(ui->imageWidget,    &FisheyeCalibrationImageWidget::animationFinished, this,  &FisheyeCalibrationWidget::at_image_animationFinished);

    init();
}

FisheyeCalibrationWidget::~FisheyeCalibrationWidget()
{
}

void FisheyeCalibrationWidget::init()
{
    ui->imageWidget->abortSearchAnimation();
    ui->imageWidget->setImage(QImage());
    setImageProvider(nullptr);
    updatePage();
}

ImageProvider* FisheyeCalibrationWidget::imageProvider() const
{
    return m_imageProvider.data();
}

// TODO: #GDM change to CameraThumbnailManager
void FisheyeCalibrationWidget::setImageProvider(ImageProvider* provider)
{
    if (m_imageProvider)
    {
        ui->imageWidget->disconnect(m_imageProvider);
        m_imageProvider->disconnect(this);
    }

    m_imageProvider = provider;

    if (!m_imageProvider)
        return;

    connect(m_imageProvider, &ImageProvider::imageChanged, ui->imageWidget,
        &FisheyeCalibrationImageWidget::setImage);
    connect(m_imageProvider, &ImageProvider::statusChanged, this,
        &FisheyeCalibrationWidget::updatePage);
    connect(m_imageProvider, &ImageProvider::imageChanged, this,
        &FisheyeCalibrationWidget::updatePage);

    updatePage();
}

QPointF FisheyeCalibrationWidget::center() const
{
    return m_calibrator->center();
}

void FisheyeCalibrationWidget::setCenter(const QPointF &center)
{
    m_calibrator->setCenter(center);
    update();
}

void FisheyeCalibrationWidget::setHorizontalStretch(const qreal &value)
{
    m_calibrator->setHorizontalStretch(value);
    update();
}

qreal FisheyeCalibrationWidget::horizontalStretch() const
{
    return m_calibrator->horizontalStretch();
}

qreal FisheyeCalibrationWidget::radius() const
{
    return m_calibrator->radius();
}

void FisheyeCalibrationWidget::setRadius(qreal radius)
{
    m_calibrator->setRadius(radius);
    update();
}

bool FisheyeCalibrationWidget::isReadOnly() const
{
    return ::isReadOnly(ui->imageWidget);
}

void FisheyeCalibrationWidget::setReadOnly(bool value)
{
    ::setReadOnly(ui->imageWidget, value);
}

void FisheyeCalibrationWidget::updatePage()
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

void FisheyeCalibrationWidget::at_calibrator_finished(int errorCode)
{
    m_lastError = errorCode;
    ui->imageWidget->endSearchAnimation();
}

void FisheyeCalibrationWidget::at_image_animationFinished()
{
    emit autoCalibrationFinished();
    const QString errorCaption = tr("Auto calibration failed");

    switch (m_lastError)
    {
        case QnFisheyeCalibrator::ErrorNotFisheyeImage:
            QnMessageBox::critical(this, errorCaption, tr("Image is not round."));
            break;

        case QnFisheyeCalibrator::ErrorTooLowLight:
            QnMessageBox::critical(this, errorCaption, tr("Image might be too dim."));
            break;
        case QnFisheyeCalibrator::ErrorInvalidInput:
            QnMessageBox::critical(this, errorCaption, tr("Invalid input image."));
            break;
        case QnFisheyeCalibrator::ErrorInternal:
            QnMessageBox::critical(this, errorCaption, tr("Internal error."));
            break;
        default:
            break;
    }
}

void FisheyeCalibrationWidget::autoCalibrate()
{
    if (isReadOnly())
        return;

    ui->imageWidget->beginSearchAnimation();
    m_calibrator->analyseFrameAsync(ui->imageWidget->image());
}

} // namespace nx::vms::client::desktop
