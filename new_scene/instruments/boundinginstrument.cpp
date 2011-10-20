#include "boundinginstrument.h"
#include <QGraphicsView>
#include <QScrollBar>

namespace {
    qreal calculateCorrection(qreal oldValue, qreal newValue, qreal lowerBound, qreal upperBound) {
        if(oldValue >= lowerBound && oldValue <= upperBound)
            return 0.0;

        return (oldValue - newValue) * 0.5;
    }

    QPointF calculateCorrection(QPointF oldValue, QPointF newValue, QRectF bounds) {
        return QPointF(
            calculateCorrection(oldValue.x(), newValue.x(), bounds.left(), bounds.right()),
            calculateCorrection(oldValue.y(), newValue.y(), bounds.top(),  bounds.bottom())
        );
    }

    qreal calculateScale(QSizeF size, QSizeF bounds) {
        return qMax(size.width() / bounds.width(), size.height() / bounds.height());
    }

    QRectF truncated(const QRectF &rect) {
        QRectF result = rect;

        if(result.width() < 0) {
            result.moveLeft(result.left() + result.width() / 2);
            result.setWidth(0);
        }

        if(result.height() < 0) {
            result.moveTop(result.top() + result.height() / 2);
            result.setHeight(0);
        }

        return result;
    }

} // anonymous namespace


BoundingInstrument::BoundingInstrument(QObject *parent):
    Instrument(makeSet(), makeSet(), makeSet(QEvent::Paint), false, parent),
    m_view(NULL)
{}

bool BoundingInstrument::paintEvent(QWidget *viewport, QPaintEvent *event) {
    if(m_view == NULL)
        return false;

    if(m_view->viewport() != viewport)
        return false;

    if(qFuzzyCompare(m_view->viewportTransform(), m_oldViewportToScene))
        return false;

    QTransform viewportToScene = m_view->viewportTransform();
    QRectF viewportRect = mapRectToScene(m_view, viewport->rect());
    qreal zoom = calculateScale(viewportRect.size(), m_zoomBounds);
    QPointF center = viewportRect.center();

    /* Calculate move bounds for viewport's center. */
    QRectF moveBounds = truncated(dilated(m_moveBounds, (m_moveBoundsExtension - 0.5) * viewportRect.size()));
    qreal zoomBound = 1.0;

    /* Calculate correction values. */
    QPointF centerCorrection = calculateCorrection(m_oldCenter, center, moveBounds);
    qreal zoomCorrection = calculateCorrection(m_oldZoom, zoom, 0, zoomBound);

    /* Apply them. */
    qreal scale = zoom / (zoom + zoomCorrection);

    QGraphicsView::ViewportAnchor anchor = m_view->transformationAnchor();
    bool interactive = m_view->isInteractive();
    m_view->setInteractive(false); /* View will re-fire stored mouse event if we don't do this. */
    m_view->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    m_view->scale(scale, scale);
    m_view->setTransformationAnchor(anchor);
    m_view->setInteractive(interactive);
    
    QScrollBar *hBar = m_view->horizontalScrollBar();
    QScrollBar *vBar = m_view->verticalScrollBar();
    QPoint delta = m_view->mapFromScene(-centerCorrection) - m_view->mapFromScene(QPointF(0.0, 0.0));
    hBar->setValue(hBar->value() + (m_view->isRightToLeft() ? delta.x() : -delta.x()));
    vBar->setValue(vBar->value() - delta.y());

    m_oldViewportToScene = m_view->viewportTransform();
    m_oldZoom = zoom + zoomCorrection;
    m_oldCenter = center + centerCorrection;

    return false;
}

void BoundingInstrument::setView(QGraphicsView *view) {
    m_view = view;

    qreal t = 10000000000.0;

    m_moveBounds = QRectF(QPointF(-t, -t), QPointF(t, t));
    m_moveBoundsExtension = 0.0;
    m_moveSpeed = 0.0;
    m_moveSpeedExtension = 1.0;

    m_zoomBounds = QSizeF(t, t);
    m_zoomSpeed = 2.0;

    m_isZoomAnimated = true;
    m_isMoveAnimated = true;

    m_oldCenter = QPointF();
    m_oldViewportToScene = QTransform();
    m_oldZoom = 1.0;
}

void BoundingInstrument::setMoveBounds(const QRectF &moveBounds, qreal extension) {
    m_moveBounds = moveBounds;
    m_moveBoundsExtension = extension;
}

void BoundingInstrument::setZoomBounds(const QSizeF &zoomBound) {
    m_zoomBounds = zoomBound;
}

void BoundingInstrument::setMoveSpeed(qreal speed, qreal extension) {
    m_moveSpeed = speed;
    m_moveSpeedExtension = extension;
}

void BoundingInstrument::setZoomSpeed(qreal multiplier) {
    m_zoomSpeed = multiplier;
}

void BoundingInstrument::setZoomAnimated(bool zoomAnimated) {
    m_isZoomAnimated = zoomAnimated;
}

void BoundingInstrument::setMoveAnimated(bool moveAnimated) {
    m_isMoveAnimated = moveAnimated;
}
