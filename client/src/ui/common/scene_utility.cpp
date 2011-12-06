#include "scene_utility.h"
#include <cassert>
#include <cmath> /* For std::sqrt, std::atan2. */
#include <QGraphicsView>
#include <QScrollBar>
#include <QCursor>
#include <utils/common/checked_cast.h>

QPointF QnSceneUtility::cwiseMul(const QPointF &l, const QPointF &r) {
    return QPointF(l.x() * r.x(), l.y() * r.y());
}

QPointF QnSceneUtility::cwiseDiv(const QPointF &l, const QPointF &r) {
    return QPointF(l.x() / r.x(), l.y() / r.y());
}

QSizeF QnSceneUtility::cwiseMul(const QSizeF &l, const QSizeF &r) {
    return QSizeF(l.width() * r.width(), l.height() * r.height());
}

QSizeF QnSceneUtility::cwiseDiv(const QSizeF &l, const QSizeF &r) {
    return QSizeF(l.width() / r.width(), l.height() / r.height());
}

MarginsF QnSceneUtility::cwiseMul(const MarginsF &l, const QSizeF &r) {
    return MarginsF(
        l.left()   * r.width(),
        l.top()    * r.height(),
        l.right()  * r.width(),
        l.bottom() * r.height()
    );
}

MarginsF QnSceneUtility::cwiseDiv(const MarginsF &l, const QSizeF &r) {
    return MarginsF(
        l.left()   / r.width(),
        l.top()    / r.height(),
        l.right()  / r.width(),
        l.bottom() / r.height()
    );
}

MarginsF QnSceneUtility::cwiseMul(const MarginsF &l, const MarginsF &r) {
    return MarginsF(
        l.left()   * r.left(), 
        l.top()    * r.top(), 
        l.right()  * r.right(), 
        l.bottom() * r.bottom()
    );
}

MarginsF QnSceneUtility::cwiseDiv(const MarginsF &l, const MarginsF &r) {
    return MarginsF(
        l.left()   / r.left(), 
        l.top()    / r.top(), 
        l.right()  / r.right(), 
        l.bottom() / r.bottom()
    );
}

QSizeF QnSceneUtility::sizeDelta(const MarginsF &margins) {
    return QSizeF(margins.left() + margins.right(), margins.top() + margins.bottom());
}

QSize QnSceneUtility::sizeDelta(const QMargins &margins) {
    return QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}

qreal QnSceneUtility::length(const QPointF &point) {
    return std::sqrt(point.x() * point.x() + point.y() * point.y());
}

qreal QnSceneUtility::length(const QSizeF &size) {
    return std::sqrt(size.width() * size.width() + size.height() * size.height());
}

QPointF QnSceneUtility::normalized(const QPointF &point) {
    return point / length(point);
}

QPointF QnSceneUtility::normal(const QPointF &point) {
    return QPointF(point.y(), -point.x());
}

qreal QnSceneUtility::atan2(const QPointF &point) {
    return std::atan2(point.y(), point.x());
}

QPointF QnSceneUtility::pointCentroid(const QPolygonF &polygon) {
    QPointF result;
    
    int size = polygon.size() - 1;
    for(int i = 0; i < size; i++)
        result += polygon[i];

    /* Add last point only if it's not equal to the first. 
     * This way both open and closed support polygons are supported. */
    if(!qFuzzyCompare(polygon[0], polygon[size])) {
        result += polygon[size];
        size++;
    }

    return result / size;
}

QRectF QnSceneUtility::mapRectToScene(QGraphicsView *view, const QRect &rect) {
    return view->viewportTransform().inverted().mapRect(QRectF(rect));
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

QSizeF QnSceneUtility::expanded(qreal aspectRatio, const QSizeF &minSize, Qt::AspectRatioMode mode) {
    if(mode == Qt::IgnoreAspectRatio)
        return minSize;

    bool expanding = mode == Qt::KeepAspectRatioByExpanding;
    bool toGreaterAspectRatio = minSize.width() / minSize.height() > aspectRatio;

    QSizeF result = minSize;
    if(expanding ^ toGreaterAspectRatio) {
        result.setWidth(result.height() * aspectRatio);
    } else {
        result.setHeight(result.width() / aspectRatio);
    }
    return result;
}

QRectF QnSceneUtility::expanded(qreal aspectRatio, const QRectF &minRect, Qt::AspectRatioMode mode, Qt::Alignment alignment) {
    if(mode == Qt::IgnoreAspectRatio)
        return minRect;

    QSizeF newSize = expanded(aspectRatio, minRect.size(), mode);
    QRectF result;
    result.setSize(newSize);

    if(alignment & Qt::AlignHCenter) {
        result.moveLeft(minRect.left() + (minRect.width() - newSize.width()) / 2);
    } else if(alignment & Qt::AlignRight) {
        result.moveRight(minRect.right());
    } else {
        result.moveLeft(minRect.left());
    }

    if(alignment & Qt::AlignVCenter) {
        result.moveTop(minRect.top() + (minRect.height() - newSize.height()) / 2);
    } else if(alignment & Qt::AlignBottom) {
        result.moveBottom(minRect.bottom());
    } else {
        result.moveTop(minRect.top());
    }

    return result;
}

QRectF QnSceneUtility::dilated(const QRectF &rect, const QSizeF &amount) {
    return rect.adjusted(-amount.width(), -amount.height(), amount.width(), amount.height());
}

QRectF QnSceneUtility::dilated(const QRectF &rect, const MarginsF &amount) {
    return rect.adjusted(-amount.left(), -amount.top(), amount.right(), amount.bottom());
}

QSizeF QnSceneUtility::dilated(const QSizeF &size, const MarginsF &amount) {
    return QSizeF(size.width() + amount.left() + amount.right(), size.height() + amount.top() + amount.bottom());
}

QRectF QnSceneUtility::eroded(const QRectF &rect, const MarginsF &amount) {
    return rect.adjusted(amount.left(), amount.top(), -amount.right(), -amount.bottom());
}

QRect QnSceneUtility::eroded(const QRect &rect, const QMargins &amount) {
    return rect.adjusted(amount.left(), amount.top(), -amount.right(), -amount.bottom());
}

bool QnSceneUtility::contains(const QSizeF &size, const QSizeF &otherSize) {
    return size.width() > otherSize.width() && size.height() > otherSize.height();
}

void QnSceneUtility::moveViewport(QGraphicsView *view, const QPoint &viewportPositionDelta) {
    assert(view != NULL);

    moveViewport(view, view->mapToScene(viewportPositionDelta) - view->mapToScene(QPoint(0, 0)));
}

void QnSceneUtility::moveViewportF(QGraphicsView *view, const QPointF &viewportPositionDelta) {
    assert(view != NULL);

    QTransform viewportToScene = view->viewportTransform().inverted();
    moveViewport(view, viewportToScene.map(viewportPositionDelta) - viewportToScene.map(QPointF(0.0, 0.0)));
}

void QnSceneUtility::moveViewport(QGraphicsView *view, const QPointF &scenePositionDelta) {
    assert(view != NULL);

    QGraphicsView::ViewportAnchor oldAnchor = view->transformationAnchor();
    bool oldInteractive = view->isInteractive();
    view->setInteractive(false); /* View will re-fire stored mouse event if we don't do this. */
    view->setTransformationAnchor(QGraphicsView::NoAnchor);

    view->translate(-scenePositionDelta.x(), -scenePositionDelta.y());

    view->setTransformationAnchor(oldAnchor);
    view->setInteractive(oldInteractive);
}

void QnSceneUtility::moveViewportTo(QGraphicsView *view, const QPointF &centerPosition) {
    assert(view != NULL);

    moveViewport(view, centerPosition - view->mapToScene(view->viewport()->rect().center()));
}

namespace {
    QPoint anchorViewportPosition(QGraphicsView *view, QGraphicsView::ViewportAnchor anchor) {
        switch(anchor) {
            case QGraphicsView::AnchorViewCenter:
                return view->viewport()->rect().center();
            case QGraphicsView::AnchorUnderMouse:
                return view->mapFromGlobal(QCursor::pos());
            default:
                return QPoint(0, 0);
        }
    }
}

void QnSceneUtility::scaleViewport(QGraphicsView *view, qreal factor, const QPoint &viewportAnchor) {
    assert(view != NULL);

    qreal sceneFactor = 1 / factor;

    QPointF oldAnchorPosition = view->mapToScene(viewportAnchor);

    QGraphicsView::ViewportAnchor oldAnchor = view->transformationAnchor();
    bool oldInteractive = view->isInteractive();
    view->setInteractive(false); /* View will re-fire stored mouse event if we don't do this. */
    view->setTransformationAnchor(QGraphicsView::NoAnchor);

    view->scale(sceneFactor, sceneFactor);
    QPointF delta = view->mapToScene(viewportAnchor) - oldAnchorPosition;
    view->translate(delta.x(), delta.y());

    view->setTransformationAnchor(oldAnchor);
    view->setInteractive(oldInteractive);
}

void QnSceneUtility::scaleViewport(QGraphicsView *view, qreal factor, QGraphicsView::ViewportAnchor anchor) {
    return scaleViewport(view, factor, anchorViewportPosition(view, anchor));
}

void QnSceneUtility::scaleViewportTo(QGraphicsView *view, const QSizeF &size, Qt::AspectRatioMode mode, QGraphicsView::ViewportAnchor anchor) {
    assert(view != NULL);

    qreal factor = scaleFactor(mapRectToScene(view, view->viewport()->rect()).size(), size, mode);

    scaleViewport(view, factor, anchor);
}

QGraphicsView *QnSceneUtility::view(QWidget *viewport) {
    assert(viewport != NULL);

    return checked_cast<QGraphicsView *>(viewport->parent());
}
