#define _USE_MATH_DEFINES
#include <cmath>

#include "fisheye_calibration_image_widget.h"

#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtCore/QAbstractAnimation>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QParallelAnimationGroup>

#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QPainter>

#include <ui/processors/drag_info.h>
#include <ui/processors/drag_processor.h>
#include <ui/style/globals.h>
#include <ui/style/helper.h>

#include <utils/common/scoped_painter_rollback.h>
#include <nx/utils/math/fuzzy.h>
#include <utils/math/color_transformations.h>

namespace {

const int animDuration = 500;

static constexpr int kLatitudeStepDegrees = 15;
static constexpr int kLongitudeStepDegrees = 15;

qreal radians(qreal degrees)
{
    return degrees / 180.0 * M_PI;
}

qreal latitudeRadius(qreal degreesFromPole, Qn::FisheyeCameraProjection projection)
{
    static const qreal kSqrt2 = sqrt(2.0);
    switch (projection)
    {
        case Qn::FisheyeCameraProjection::equidistant:
            return degreesFromPole / 90.0;

        case Qn::FisheyeCameraProjection::stereographic:
            return tan(radians(degreesFromPole) / 2.0);

        case Qn::FisheyeCameraProjection::equisolid:
            return sin(radians(degreesFromPole) / 2.0) * kSqrt2;
    }

    NX_ASSERT(false);
    return 0;
}

} // namespace

namespace nx::vms::client::desktop {

FisheyeCalibrationImageWidget::FisheyeCalibrationImageWidget(QWidget *parent):
    QWidget(parent),
    m_dragProcessor(new DragProcessor(this)),
    m_frameColor(Qt::lightGray),
    m_lineColor(QColor(128, 196, 255)),
    m_center(0.5, 0.5),
    m_radius(0.5),
    m_stretch(1.0),
    m_lineWidth(4)
{
    m_dragProcessor->setHandler(this);
    setMouseTracking(true);

    m_animation.stage = Idle;
    m_animation.circle = new FisheyeAnimatedCircle(this);
    connect(m_animation.circle, SIGNAL(changed()), this, SLOT(repaint()));

    setAutoFillBackground(false);
}

FisheyeCalibrationImageWidget::~FisheyeCalibrationImageWidget()
{
}

QImage FisheyeCalibrationImageWidget::image() const
{
    return m_image;
}

void FisheyeCalibrationImageWidget::setImage(const QImage &image)
{
    m_image = image;
    m_cachedImage = QImage();
    repaint();
}

QColor FisheyeCalibrationImageWidget::frameColor() const
{
    return m_frameColor;
}

void FisheyeCalibrationImageWidget::setFrameColor(const QColor &color)
{
    if (m_frameColor == color)
        return;
    m_frameColor = color;
    repaint();
}

QColor FisheyeCalibrationImageWidget::lineColor() const
{
    return m_lineColor;
}

void FisheyeCalibrationImageWidget::setLineColor(const QColor &color)
{
    if (m_lineColor == color)
        return;
    m_lineColor = color;
    repaint();
}

QPointF FisheyeCalibrationImageWidget::center() const
{
    return m_center;
}

void FisheyeCalibrationImageWidget::setCenter(const QPointF &center)
{
    if (qFuzzyEquals(m_center, center))
        return;
    m_center = center;
    repaint();
}

qreal FisheyeCalibrationImageWidget::radius() const
{
    return m_radius;
}

void FisheyeCalibrationImageWidget::setRadius(qreal radius)
{
    if (qFuzzyEquals(m_radius, radius))
        return;
    m_radius = radius;
    repaint();
}

qreal FisheyeCalibrationImageWidget::stretch() const
{
    return m_stretch;
}

void FisheyeCalibrationImageWidget::setStretch(qreal stretch)
{
    if (qFuzzyEquals(m_stretch, stretch))
        return;
    m_stretch = stretch;
    repaint();
}

int FisheyeCalibrationImageWidget::lineWidth() const
{
    return m_lineWidth;
}

void FisheyeCalibrationImageWidget::setLineWidth(int width)
{
    if (m_lineWidth == width)
        return;
    m_lineWidth = width;
    repaint();
}

bool FisheyeCalibrationImageWidget::isGridShown() const
{
    return m_isGridShown;
}

void FisheyeCalibrationImageWidget::setGridShown(bool value)
{
    if (m_isGridShown == value)
        return;

    m_isGridShown = value;
    update();
}

Qn::FisheyeCameraProjection FisheyeCalibrationImageWidget::projection() const
{
    return m_projection;
}

void FisheyeCalibrationImageWidget::setProjection(Qn::FisheyeCameraProjection value)
{
    if (m_projection == value)
        return;

    m_projection = value;
    if (m_isGridShown)
        update();
}

void FisheyeCalibrationImageWidget::beginSearchAnimation()
{
    if (m_image.isNull() || m_image.width() == 0)
        return;

    m_animation.circle->setCenter(m_center);
    m_animation.circle->setRadius(m_radius);
    m_animation.circle->setStretch(m_stretch);
    m_animation.stage = Searching;
}


void FisheyeCalibrationImageWidget::finishAnimationStep()
{
    QPropertyAnimation* anim1 = new QPropertyAnimation(m_animation.circle, "center", this);
    anim1->setDuration(animDuration);
    anim1->setStartValue(m_animation.circle->center());
    anim1->setEndValue(m_center);

    QPropertyAnimation* anim2 = new QPropertyAnimation(m_animation.circle, "radius", this);
    anim2->setDuration(animDuration);
    anim2->setStartValue(m_animation.circle->radius());
    anim2->setEndValue(m_radius);

    QPropertyAnimation* anim3 = new QPropertyAnimation(m_animation.circle, "stretch", this);
    anim3->setDuration(animDuration);
    anim3->setStartValue(m_animation.circle->stretch());
    anim3->setEndValue(m_stretch);

    if (m_animation.animation)
        m_animation.animation->stop();

    QParallelAnimationGroup* anim = new QParallelAnimationGroup(this);
    anim->addAnimation(anim1);
    anim->addAnimation(anim2);
    anim->addAnimation(anim3);

    connect(anim, &QAbstractAnimation::finished, this, &FisheyeCalibrationImageWidget::endAnimation);

    m_animation.animation = anim;
    m_animation.animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void FisheyeCalibrationImageWidget::endAnimation()
{
    if (m_animation.stage != Finishing)
        return;

    m_animation.stage = Idle;
    emit animationFinished();
}

void FisheyeCalibrationImageWidget::endSearchAnimation()
{
    m_animation.stage = Finishing;
    finishAnimationStep();
}

void FisheyeCalibrationImageWidget::abortSearchAnimation()
{
    m_animation.stage = Idle;
}

void FisheyeCalibrationImageWidget::paintCircle(QPainter *painter, const QRect &targetRect, const QPointF &relativeCenter, const qreal relativeRadius, const qreal xStretch)
{
    if (qFuzzyIsNull(relativeRadius))
        return;

    qreal radiusX = relativeRadius * targetRect.width();
    qreal radiusY = radiusX / xStretch;
    QPointF center(relativeCenter.x() * targetRect.width(), relativeCenter.y() * targetRect.height());
    center += targetRect.topLeft();

    {
        QPen pen;
        QnScopedPainterPenRollback penRollback(painter);
        QnScopedPainterClipRegionRollback clipRollback(painter, targetRect);

        // Drawing circles.
        pen.setWidth(m_lineWidth);
        pen.setColor(m_lineColor);
        painter->setPen(pen);

        QColor brushColor1(toTransparent(m_lineColor, 0.6));
        QColor brushColor2(toTransparent(m_lineColor, 0.3));
        QRadialGradient gradient(center * 0.66, radiusX);
        gradient.setColorAt(0, brushColor1);
        gradient.setColorAt(1, brushColor2);
        QBrush brush(gradient);

        QnScopedPainterBrushRollback brushRollback(painter, brush);

        painter->drawEllipse(center, radiusX, radiusY);
        painter->drawEllipse(center, m_lineWidth, m_lineWidth);

        if (!m_isGridShown)
            return;

        // Drawing grid.
        pen.setWidth(1.0);
        pen.setColor(m_lineColor);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);

        for (int longitude = 0; longitude < 360; longitude += kLongitudeStepDegrees)
        {
            const auto angle = radians(longitude);
            const auto x = cos(angle);
            const auto y = sin(angle);

            painter->drawLine(
                center + QPointF(m_lineWidth * x, m_lineWidth * y),
                center + QPointF(radiusX * x, radiusY * y));
        }

        for (int latitude = 0; latitude < 90; latitude += kLatitudeStepDegrees)
        {
            const qreal radius = latitudeRadius(latitude, m_projection);
            painter->drawEllipse(center, radiusX * radius, radiusY * radius);
        }
    }
}

/* Handlers */

void FisheyeCalibrationImageWidget::mousePressEvent(QMouseEvent *event)
{
    base_type::mousePressEvent(event);
    m_dragProcessor->widgetMousePressEvent(this, event);
}

void FisheyeCalibrationImageWidget::mouseMoveEvent(QMouseEvent *event)
{
    base_type::mouseMoveEvent(event);
    m_dragProcessor->widgetMouseMoveEvent(this, event);
}

void FisheyeCalibrationImageWidget::mouseReleaseEvent(QMouseEvent *event)
{
    base_type::mouseReleaseEvent(event);
    m_dragProcessor->widgetMouseReleaseEvent(this, event);
}

void FisheyeCalibrationImageWidget::wheelEvent(QWheelEvent *event)
{
    if (m_cachedImage.isNull())
        return;

    qreal radius = m_radius;
    radius += 0.01 / 120 * (event->angleDelta().y());   // one wheel point is about 15 degrees, angleDelta is returned as eighths of a degree
    emit radiusModified(radius);
}

void FisheyeCalibrationImageWidget::dragMove(DragInfo *info)
{
    if (m_cachedImage.isNull())
        return;

    qreal xOffset = (qreal)(info->mouseScreenPos().x() - info->lastMouseScreenPos().x()) / m_cachedImage.width();
    qreal yOffset = (qreal)(info->mouseScreenPos().y() - info->lastMouseScreenPos().y()) / m_cachedImage.height();

    QPointF newCenter(m_center.x() + xOffset, m_center.y() + yOffset);
    emit centerModified(newCenter);
}

void FisheyeCalibrationImageWidget::paintEvent(QPaintEvent* /*event*/)
{
    if (m_image.isNull())
        return;

    QScopedPointer<QPainter> painter(new QPainter(this));

    if (!isEnabled())
        painter->setOpacity(style::Hints::kDisabledItemOpacity);

    painter->fillRect(rect(), palette().color(QPalette::Window));

    QRect targetRect = rect().adjusted(1, 1, -1, -1);

    if (targetRect != m_cachedRect || m_cachedImage.isNull())
    {
        m_cachedRect = targetRect;
        m_cachedImage = m_image.scaled(targetRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    /* Update target rect to be in the center of the source rect and match the image size */
    targetRect.setSize(m_cachedImage.size());
    targetRect.moveLeft((rect().width() - targetRect.width()) / 2);
    targetRect.moveTop((rect().height() - targetRect.height()) / 2);

    painter->drawImage(targetRect, m_cachedImage);

    painter->setRenderHints(QPainter::Antialiasing | QPainter::HighQualityAntialiasing);

    if (m_lineWidth == 0)
        return;

    if (m_animation.stage == Idle)
        paintCircle(painter.data(), targetRect, m_center, m_radius, m_stretch);
    else
        paintCircle(painter.data(), targetRect, m_animation.circle->center(), m_animation.circle->radius(), m_animation.circle->stretch());
}

} // namespace nx::vms::client::desktop
