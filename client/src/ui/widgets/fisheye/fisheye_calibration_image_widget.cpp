#include "fisheye_calibration_image_widget.h"

#include <QtCore/QDateTime>
#include <QtCore/QTimer>

#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QPainter>

#include <ui/processors/drag_info.h>
#include <ui/processors/drag_processor.h>
#include <ui/style/globals.h>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/fuzzy.h>
#include <utils/math/color_transformations.h>


QnFisheyeCalibrationImageWidget::QnFisheyeCalibrationImageWidget(QWidget *parent) :
    QWidget(parent),
    m_dragProcessor(new DragProcessor(this)),
    m_frameColor(Qt::lightGray),
    m_lineColor(qnGlobals->ptzColor()),
    m_center(0.5, 0.5),
    m_radius(0.5),
    m_lineWidth(4)
{
    m_dragProcessor->setHandler(this);
    setMouseTracking(true);

    m_animation.stage = Idle;
}

QnFisheyeCalibrationImageWidget::~QnFisheyeCalibrationImageWidget() {
}

QImage QnFisheyeCalibrationImageWidget::image() const {
    return m_image;
}

void QnFisheyeCalibrationImageWidget::setImage(const QImage &image) {
    m_image = image;
    repaint();
}

QColor QnFisheyeCalibrationImageWidget::frameColor() const {
    return m_frameColor;
}

void QnFisheyeCalibrationImageWidget::setFrameColor(const QColor &color) {
    if (m_frameColor == color)
        return;
    m_frameColor = color;
    repaint();
}

QColor QnFisheyeCalibrationImageWidget::lineColor() const {
    return m_lineColor;
}

void QnFisheyeCalibrationImageWidget::setLineColor(const QColor &color) {
    if (m_lineColor == color)
        return;
    m_lineColor = color;
    repaint();
}

QPointF QnFisheyeCalibrationImageWidget::center() const {
    return m_center;
}

void QnFisheyeCalibrationImageWidget::setCenter(const QPointF &center) {
    if (qFuzzyEquals(m_center, center))
        return;
    m_center = center;
    repaint();
}

qreal QnFisheyeCalibrationImageWidget::radius() const {
    return m_radius;
}

void QnFisheyeCalibrationImageWidget::setRadius(qreal radius) {
    if (qFuzzyEquals(m_radius, radius))
        return;
    m_radius = radius;
    repaint();
}

int QnFisheyeCalibrationImageWidget::lineWidth() const {
    return m_lineWidth;
}

void QnFisheyeCalibrationImageWidget::setLineWidth(int width) {
    if (m_lineWidth == width)
        return;
    m_lineWidth = width;
    repaint();
}


void QnFisheyeCalibrationImageWidget::beginSearchAnimation() {
    m_animation.stage = Searching;
    m_animation.timestamp = QDateTime::currentMSecsSinceEpoch();
//    m_animation.center = m_ce
}

void QnFisheyeCalibrationImageWidget::endSearchAnimation() {
    m_animation.stage = Idle;
}

void QnFisheyeCalibrationImageWidget::paintCircle(QPainter *painter, const QRect &targetRect, const QPointF &relativeCenter, const qreal relativeRadius, bool paintCenter) {
    if (qFuzzyIsNull(relativeRadius))
        return;

    int halfLineWidth = m_lineWidth / 2;
    qreal radius = relativeRadius * targetRect.width();
    QPointF center(relativeCenter.x() * targetRect.width(), relativeCenter.y() * targetRect.height());
    center += targetRect.topLeft();

    {   /* Drawing circles */
        QPen pen;
        pen.setWidth(m_lineWidth);
        pen.setColor(m_lineColor);
        QnScopedPainterPenRollback penRollback(painter, pen);
        Q_UNUSED(penRollback)

        QColor brushColor1(toTransparent(m_lineColor, 0.6));
        QColor brushColor2(toTransparent(m_lineColor, 0.3));
        QRadialGradient gradient(center * 0.66, radius);
        gradient.setColorAt(0, brushColor1);
        gradient.setColorAt(1, brushColor2);
        QBrush brush(gradient);

        QnScopedPainterBrushRollback brushRollback(painter, brush);
        Q_UNUSED(brushRollback)

        QPainterPath path;
        path.addEllipse(center, radius, radius);
        if (paintCenter)
            path.addEllipse(center, m_lineWidth, m_lineWidth);

        /* Adjust again to not draw over frame */
        painter->setClipRect(targetRect.adjusted(halfLineWidth, halfLineWidth, -halfLineWidth, -halfLineWidth));
        painter->drawPath(path);
        painter->setClipping(false);
    }
}

/* Handlers */

void QnFisheyeCalibrationImageWidget::mousePressEvent(QMouseEvent *event) {
    base_type::mousePressEvent(event);
    m_dragProcessor->widgetMousePressEvent(this, event);
}

void QnFisheyeCalibrationImageWidget::mouseMoveEvent(QMouseEvent *event) {
    base_type::mouseMoveEvent(event);
    m_dragProcessor->widgetMouseMoveEvent(this, event);
}

void QnFisheyeCalibrationImageWidget::mouseReleaseEvent(QMouseEvent *event) {
    base_type::mouseReleaseEvent(event);
    m_dragProcessor->widgetMouseReleaseEvent(this, event);
}

void QnFisheyeCalibrationImageWidget::wheelEvent(QWheelEvent *event) {
    qreal radius = m_radius;
    radius += 0.01 / 120 * (event->angleDelta().y());   // one wheel point is about 15 degrees, angleDelta is returned as eighths of a degree
    emit radiusModified(radius);
}

void QnFisheyeCalibrationImageWidget::dragMove(DragInfo *info) {
    if (m_cachedImage.isNull())
        return;

    qreal xOffset = (qreal)(info->mouseScreenPos().x() - info->lastMouseScreenPos().x()) / m_cachedImage.width();
    qreal yOffset = (qreal)(info->mouseScreenPos().y() - info->lastMouseScreenPos().y()) / m_cachedImage.height();

    QPointF newCenter(m_center.x() + xOffset, m_center.y() + yOffset);
    emit centerModified(newCenter);
}

void QnFisheyeCalibrationImageWidget::paintEvent(QPaintEvent *event) {
    if (m_image.isNull())
        return;

    QScopedPointer<QPainter> painter(new QPainter(this));

    int halfLineWidth = m_lineWidth / 2;
    QRect targetRect = event->rect().adjusted(halfLineWidth, halfLineWidth, -halfLineWidth, -halfLineWidth);

    if (targetRect != m_cachedRect) {
        m_cachedRect = targetRect;
        m_cachedImage = m_image.scaled(targetRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    /* Update target rect to be in the center of the source rect and match the image size */
    targetRect.setSize(m_cachedImage.size());
    targetRect.moveLeft( (event->rect().width() - targetRect.width()) / 2 );
    targetRect.moveTop( (event->rect().height() - targetRect.height()) / 2 );

    painter->drawImage(targetRect, m_cachedImage);

    painter->setRenderHints(QPainter::Antialiasing | QPainter::HighQualityAntialiasing);

    if (m_lineWidth == 0)
        return;

    {   /* Drawing frame */
        QPen pen;
        pen.setWidth(m_lineWidth);
        pen.setColor(m_frameColor);
        QnScopedPainterPenRollback penRollback(painter.data(), pen);
        Q_UNUSED(penRollback)

        painter->drawRect(targetRect);
    }

    switch (m_animation.stage) {
    case Idle:
        paintCircle(painter.data(), targetRect, m_center, m_radius, true);
        break;
    case Searching:
        const qreal speed = 0.5;
        const qreal space = 0.3;

        qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
        qint64 elapsed = timestamp - m_animation.timestamp;
        qreal step = (speed * elapsed / 1e-3);

        qreal outerWave = 0;
        QList<qreal> waves;
        foreach (qreal prevRadius, m_animation.waves) {
            qreal newRadius = prevRadius - step;
            if (newRadius > 0) {
                waves << newRadius;
                outerWave = newRadius;
            }
        }

        qreal nextWave = outerWave + space;
        while (nextWave < 1.0) {
            waves << nextWave;
            nextWave += space;
        }
        m_animation.waves = waves;
        foreach (qreal radius, m_animation.waves)
            paintCircle(painter.data(), targetRect, QPointF(0.5, 0.5), radius);

        m_animation.timestamp = timestamp;
        QTimer::singleShot(16, Qt::PreciseTimer, this, SLOT(repaint()));
        break;
    }


}
