#include "scene_utility.h"
#include <cassert>
#include <QGraphicsView>
#include <QScrollBar>
#include <QCursor>

QRectF QnSceneUtility::mapRectToScene(QGraphicsView *view, const QRect &rect) {
    return view->viewportTransform().inverted().mapRect(rect);
}

QRect QnSceneUtility::mapRectFromScene(QGraphicsView *view, const QRectF &rect) {
    return view->viewportTransform().mapRect(rect).toRect();
}

qreal QnSceneUtility::scaleFactor(QSizeF size, QSizeF bounds, Qt::AspectRatioMode mode) {
    if(mode == Qt::KeepAspectRatioByExpanding) {
        return qMax(bounds.width() / size.width(), bounds.height() / size.height());
    } else {
        return qMin(bounds.width() / size.width(), bounds.height() / size.height());
    }
}

QSizeF QnSceneUtility::bounded(const QSizeF &size, const QSizeF &maxSize, Qt::AspectRatioMode mode) {
    if(mode == Qt::IgnoreAspectRatio)
        return size.boundedTo(maxSize);

    qreal factor = scaleFactor(size, maxSize, mode);
    if(factor >= 1.0)
        return size;

    return size * factor;
}

QSizeF QnSceneUtility::expanded(const QSizeF &size, const QSizeF &minSize, Qt::AspectRatioMode mode) {
    if(mode == Qt::IgnoreAspectRatio)
        return size.expandedTo(minSize);

    qreal factor = scaleFactor(size, minSize, mode);
    if(factor <= 1.0)
        return size;

    return size * factor;
}

QRectF QnSceneUtility::dilated(const QRectF &rect, const QSizeF &amount) {
    return rect.adjusted(-amount.width(), -amount.height(), amount.width(), amount.height());
}

bool QnSceneUtility::contains(const QSizeF &size, const QSizeF &otherSize) {
    return size.width() > otherSize.width() && size.height() > otherSize.height();
}

void QnSceneUtility::moveViewport(QGraphicsView *view, const QPoint &positionDelta) {
    assert(view != NULL);

    moveViewport(view, view->mapToScene(positionDelta) - view->mapToScene(QPoint(0, 0)));
}

void QnSceneUtility::moveViewport(QGraphicsView *view, const QPointF &positionDelta) {
    assert(view != NULL);

    QGraphicsView::ViewportAnchor oldAnchor = view->transformationAnchor();
    bool oldInteractive = view->isInteractive();
    view->setInteractive(false); /* View will re-fire stored mouse event if we don't do this. */
    view->setTransformationAnchor(QGraphicsView::NoAnchor);

    view->translate(-positionDelta.x(), -positionDelta.y());

    view->setTransformationAnchor(oldAnchor);
    view->setInteractive(oldInteractive);
}

void QnSceneUtility::moveViewportTo(QGraphicsView *view, const QPointF &centerPosition) {
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

void QnSceneUtility::scaleViewport(QGraphicsView *view, qreal factor, QGraphicsView::ViewportAnchor anchor) {
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

void QnSceneUtility::scaleViewportTo(QGraphicsView *view, const QSizeF &size, Qt::AspectRatioMode mode, QGraphicsView::ViewportAnchor anchor) {
    assert(view != NULL);

    qreal factor = scaleFactor(mapRectToScene(view, view->viewport()->rect()).size(), size, mode);

    scaleViewport(view, factor, anchor);
}
