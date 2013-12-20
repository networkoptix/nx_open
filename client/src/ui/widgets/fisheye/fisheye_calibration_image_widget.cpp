#include "fisheye_calibration_image_widget.h"

#include <QtCore/QEvent>
#include <QtGui/QPainter>

#include <ui/style/globals.h>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/fuzzy.h>
#include <utils/math/color_transformations.h>


QnFisheyeCalibrationImageWidget::QnFisheyeCalibrationImageWidget(QWidget *parent) :
    QWidget(parent),
    m_frameColor(Qt::lightGray),
    m_lineColor(qnGlobals->ptzColor()),
    m_center(0.5, 0.5),
    m_radius(0.5),
    m_lineWidth(4)
{
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

void QnFisheyeCalibrationImageWidget::paintEvent(QPaintEvent *event) {
    if (m_image.isNull())
        return;

    QScopedPointer<QPainter> painter(new QPainter(this));

    int halfLineWidth = m_lineWidth / 2;
    QRect targetRect = event->rect().adjusted(halfLineWidth, halfLineWidth, -halfLineWidth, -halfLineWidth);

    /* Update target rect to be in the center of the source rect and match the image size */
    QImage targetImage = m_image.scaled(targetRect.size(), Qt::KeepAspectRatio, Qt::FastTransformation);
    targetRect.setSize(targetImage.size());
    targetRect.moveLeft( (event->rect().width() - targetRect.width()) / 2 );
    targetRect.moveTop( (event->rect().height() - targetRect.height()) / 2 );

    painter->drawImage(targetRect, m_image);

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

    if (qFuzzyIsNull(m_radius))
        return;

    qreal radius = m_radius * targetRect.width();
    QPointF center(m_center.x() * targetRect.width(), m_center.y() * targetRect.height());
    center += targetRect.topLeft();

    {   /* Drawing circles */
        QPen pen;
        pen.setWidth(m_lineWidth);
        pen.setColor(m_lineColor);
        QnScopedPainterPenRollback penRollback(painter.data(), pen);
        Q_UNUSED(penRollback)

        QnScopedPainterBrushRollback brushRollback(painter.data(), toTransparent(m_lineColor, 0.5));
        Q_UNUSED(brushRollback)

        QPainterPath path;
        path.addEllipse(center, radius, radius);
        path.addEllipse(center, m_lineWidth, m_lineWidth);

        /* Adjust again to not draw over frame */
        painter->setClipRect(targetRect.adjusted(halfLineWidth, halfLineWidth, -halfLineWidth, -halfLineWidth));
        painter->drawPath(path);
        painter->setClipping(false);
    }
}
