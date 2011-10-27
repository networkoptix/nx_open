#include "instrumentutility.h"
#include <cassert>
#include <QGraphicsView>
#include <QScrollBar>
#include <QCursor>

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

bool InstrumentUtility::contains(const QSizeF &size, const QSizeF &otherSize) {
    return size.width() > otherSize.width() && size.height() > otherSize.height();
}

void InstrumentUtility::moveViewport(QGraphicsView *view, const QPoint &positionDelta) {
    assert(view != NULL);

    moveViewport(view, view->mapToScene(positionDelta) - view->mapToScene(QPoint(0, 0)));
}

void InstrumentUtility::moveViewport(QGraphicsView *view, const QPointF &positionDelta) {
    assert(view != NULL);

    QGraphicsView::ViewportAnchor oldAnchor = view->transformationAnchor();
    bool oldInteractive = view->isInteractive();
    view->setInteractive(false); /* View will re-fire stored mouse event if we don't do this. */
    view->setTransformationAnchor(QGraphicsView::NoAnchor);

    view->translate(-positionDelta.x(), -positionDelta.y());

    view->setTransformationAnchor(oldAnchor);
    view->setInteractive(oldInteractive);
}

void InstrumentUtility::moveViewportTo(QGraphicsView *view, const QPointF &centerPosition) {
    assert(view != NULL);

    moveViewport(view, centerPosition - view->mapToScene(view->viewport()->rect().center()));
}

namespace {
    QPointF anchorScenePosition(QGraphicsView *view, QGraphicsView::ViewportAnchor anchor) {
        switch(anchor) {
            case QGraphicsView::AnchorViewCenter:
                return view->mapToScene(view->viewport()->rect().center());
            case QGraphicsView::AnchorUnderMouse:
                return view->mapToScene(view->mapFromGlobal(QCursor::pos()));
            default:
                return QPointF();
        }
    }
}

void InstrumentUtility::scaleViewport(QGraphicsView *view, qreal factor, QGraphicsView::ViewportAnchor anchor) {
    assert(view != NULL);

    qreal sceneFactor = 1 / factor;

    QPointF oldAnchorPosition = anchorScenePosition(view, anchor);
    
    QGraphicsView::ViewportAnchor oldAnchor = view->transformationAnchor();
    bool oldInteractive = view->isInteractive();
    view->setInteractive(false); /* View will re-fire stored mouse event if we don't do this. */
    view->setTransformationAnchor(QGraphicsView::NoAnchor);

    view->scale(sceneFactor, sceneFactor);
    QPointF delta = anchorScenePosition(view, anchor) - oldAnchorPosition;
    view->translate(delta.x(), delta.y());

    view->setTransformationAnchor(oldAnchor);
    view->setInteractive(oldInteractive);
}

void InstrumentUtility::scaleViewportTo(QGraphicsView *view, const QSizeF &size, BoundingMode mode, QGraphicsView::ViewportAnchor anchor) {
    assert(view != NULL);

    qreal factor = 1.0 / calculateScale(mapRectToScene(view, view->viewport()->rect()).size(), size, mode);

    scaleViewport(view, factor, anchor);
}

qreal InstrumentUtility::calculateScale(QSizeF size, QSizeF bounds, BoundingMode mode) {
    if(mode == InstrumentUtility::InBound) {
        return qMax(size.width() / bounds.width(), size.height() / bounds.height());
    } else {
        return qMin(size.width() / bounds.width(), size.height() / bounds.height());
    }
}
