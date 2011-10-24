#include "instrumentutility.h"
#include <cassert>
#include <QGraphicsView>
#include <QScrollBar>

QRectF InstrumentUtility::mapRectToScene(QGraphicsView *view, const QRect &rect) {
    return view->viewportTransform().inverted().mapRect(rect);
}

QRect InstrumentUtility::mapRectFromScene(QGraphicsView *view, const QRectF &rect) {
    return view->viewportTransform().mapRect(rect).toRect();
}

QSizeF InstrumentUtility::bounded(const QSizeF &size, const QSizeF &maxSize, Qt::AspectRatioMode mode) {
    if(mode == Qt::IgnoreAspectRatio)
        return size.boundedTo(maxSize);

    qreal ratio = qMin(maxSize.width() / size.width(), maxSize.height() / size.height());
    if(ratio >= 1.0)
        return size;

    return size * ratio;
}

QSizeF InstrumentUtility::expanded(const QSizeF &size, const QSizeF &minSize, Qt::AspectRatioMode mode) {
    if(mode == Qt::IgnoreAspectRatio)
        return size.expandedTo(minSize);

    qreal ratio = qMax(minSize.width() / size.width(), minSize.height() / size.height());
    if(ratio <= 1.0)
        return size;

    return size * ratio;
}

QRectF InstrumentUtility::dilated(const QRectF &rect, const QSizeF &amount) {
    return rect.adjusted(-amount.width(), -amount.height(), amount.width(), amount.height());
}

void InstrumentUtility::moveViewport(QGraphicsView *view, const QPoint &positionDelta) {
    assert(view != NULL);

    QScrollBar *hBar = view->horizontalScrollBar();
    QScrollBar *vBar = view->verticalScrollBar();
    hBar->setValue(hBar->value() + (view->isRightToLeft() ? -positionDelta.x() : positionDelta.x()));
    vBar->setValue(vBar->value() + positionDelta.y());
}

void InstrumentUtility::moveViewport(QGraphicsView *view, const QTransform &sceneToViewport, const QPointF &positionDelta) {
    moveViewport(view, (sceneToViewport.map(positionDelta) - sceneToViewport.map(QPointF(0.0, 0.0))).toPoint());
}

void InstrumentUtility::moveViewport(QGraphicsView *view, const QPointF &positionDelta) {
    moveViewport(view, view->viewportTransform(), positionDelta);
}

void InstrumentUtility::scaleViewport(QGraphicsView *view, qreal factor) {
    assert(view != NULL);

    qreal sceneFactor = 1 / factor;

    QGraphicsView::ViewportAnchor anchor = view->transformationAnchor();
    bool interactive = view->isInteractive();
    view->setInteractive(false); /* View will re-fire stored mouse event if we don't do this. */
    view->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    view->scale(sceneFactor, sceneFactor);
    view->setTransformationAnchor(anchor);
    view->setInteractive(interactive);
}

qreal InstrumentUtility::calculateScale(QSizeF size, QSizeF bounds, BoundingMode mode) {
    if(mode == InstrumentUtility::InBound) {
        return qMax(size.width() / bounds.width(), size.height() / bounds.height());
    } else {
        return qMin(size.width() / bounds.width(), size.height() / bounds.height());
    }
}
