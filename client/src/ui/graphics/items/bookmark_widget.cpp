#include "bookmark_widget.h"
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/warnings.h>

namespace {
    const qreal roundingRadius = 5.0;
}

QnBookmarkWidget::QnBookmarkWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    QGraphicsWidget(parent, windowFlags),
    m_shape(RoundedNorth),
    m_frameWidth(0),
    m_shapeValid(false)
{}

QnBookmarkWidget::~QnBookmarkWidget() {
    return;
}

void QnBookmarkWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    ensureShape();

    QPalette palette = this->palette();
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);
    QnScopedPainterBrushRollback brushRollback(painter, palette.brush(QPalette::Window));
    QnScopedPainterPenRollback penRollback(painter, QPen(palette.brush(QPalette::WindowText), m_frameWidth));
    painter->drawPath(m_borderShape);
}

void QnBookmarkWidget::changeEvent(QEvent *event) {
    if(event->type() == QEvent::PaletteChange)
        update();

    base_type::changeEvent(event);
}

void QnBookmarkWidget::resizeEvent(QGraphicsSceneResizeEvent *event) {
    invalidateShape();
    update();

    base_type::resizeEvent(event);
}

void QnBookmarkWidget::setBookmarkShape(Shape shape) {
    if(m_shape == shape)
        return;

    m_shape = shape;
    invalidateShape();
    update();
}

void QnBookmarkWidget::setFrameWidth(qreal frameWidth) {
    if(qFuzzyCompare(m_frameWidth, frameWidth))
        return;

    m_frameWidth = frameWidth;
    update();
}

void QnBookmarkWidget::invalidateShape() {
    m_shapeValid = false;
}

void QnBookmarkWidget::ensureShape() const {
    if(m_shapeValid)
        return;

    QPointF o, x, y, xr, yr;
    qreal d;
    switch(m_shape) {
    case RoundedNorth:
        d = 90;
        o = QPointF(0, size().height());
        x = QPointF(0, -size().height());
        y = QPointF(size().width(), 0);
        xr = QPointF(0, -roundingRadius);
        yr = QPointF(roundingRadius, 0);
        break;
    case RoundedSouth:
        d = -90;
        o = QPointF(size().width(), 0);
        x = QPointF(0, size().height());
        y = QPointF(-size().width(), 0);
        xr = QPointF(0, roundingRadius);
        yr = QPointF(-roundingRadius, 0);
        break;
    case RoundedEast:
        d = 0;
        o = QPointF(0, 0);
        x = QPointF(size().width(), 0);
        y = QPointF(0, size().height());
        xr = QPointF(roundingRadius, 0);
        yr = QPointF(0, roundingRadius);
        break;
    case RoundedWest:
        d = 180;
        o = QPointF(size().width(), size().height());
        x = QPointF(-size().width(), 0);
        y = QPointF(0, -size().height());
        xr = QPointF(-roundingRadius, 0);
        yr = QPointF(0, -roundingRadius);
        break;
    default:
        qnWarning("Unreachable code executed");
        break;
    }

    /* Draw as RoundedEast. */
    m_borderShape = QPainterPath();
    m_borderShape.moveTo(o);
    m_borderShape.lineTo(o + x - xr);
    m_borderShape.arcTo(QRectF(o + x - 2 * xr, o + x + 2 * yr).normalized(), d + 90, -90);
    m_borderShape.lineTo(o + x + y - yr);
    m_borderShape.arcTo(QRectF(o + x + y - 2 * (xr + yr), o + x + y).normalized(), d + 0, -90);
    m_borderShape.lineTo(o + y);
    m_borderShape.closeSubpath();

    m_shapeValid = true;
}



