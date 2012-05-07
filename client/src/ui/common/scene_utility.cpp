#include "scene_utility.h"
#include <cassert>
#include <cmath> /* For std::sqrt, std::atan2. */
#include <QGraphicsView>
#include <QScrollBar>
#include <QCursor>
#include <utils/common/checked_cast.h>
#include <utils/common/warnings.h>
#include <utils/common/util.h>

namespace {
#ifdef QN_SCENE_UTILITY_DEBUG
    void debugViewportRect(QGraphicsView *view) {
        QRectF rect = SceneUtility::mapRectToScene(view, view->viewport()->rect());
        qDebug() << "SCENE VIEWPORT RECT:" << rect;
    }
#else
#   define debugViewportRect(...)
#endif

} // anonymous namespace


QPointF SceneUtility::cwiseMul(const QPointF &l, const QPointF &r) {
    return QPointF(l.x() * r.x(), l.y() * r.y());
}

QPointF SceneUtility::cwiseDiv(const QPointF &l, const QPointF &r) {
    return QPointF(l.x() / r.x(), l.y() / r.y());
}

QSizeF SceneUtility::cwiseMul(const QSizeF &l, const QSizeF &r) {
    return QSizeF(l.width() * r.width(), l.height() * r.height());
}

QSizeF SceneUtility::cwiseDiv(const QSizeF &l, const QSizeF &r) {
    return QSizeF(l.width() / r.width(), l.height() / r.height());
}

MarginsF SceneUtility::cwiseMul(const MarginsF &l, const QSizeF &r) {
    return MarginsF(
        l.left()   * r.width(),
        l.top()    * r.height(),
        l.right()  * r.width(),
        l.bottom() * r.height()
    );
}

MarginsF SceneUtility::cwiseDiv(const MarginsF &l, const QSizeF &r) {
    return MarginsF(
        l.left()   / r.width(),
        l.top()    / r.height(),
        l.right()  / r.width(),
        l.bottom() / r.height()
    );
}

MarginsF SceneUtility::cwiseMul(const QSizeF &l, const MarginsF &r) {
    return MarginsF(
        l.width()  * r.left(),
        l.height() * r.top(),
        l.width()  * r.right(),
        l.height() * r.bottom()
    );
}

MarginsF SceneUtility::cwiseDiv(const QSizeF &l, const MarginsF &r) {
    return MarginsF(
        l.width()  / r.left(),
        l.height() / r.top(),
        l.width()  / r.right(),
        l.height() / r.bottom()
    );
}

MarginsF SceneUtility::cwiseMul(const MarginsF &l, const MarginsF &r) {
    return MarginsF(
        l.left()   * r.left(), 
        l.top()    * r.top(), 
        l.right()  * r.right(), 
        l.bottom() * r.bottom()
    );
}

MarginsF SceneUtility::cwiseDiv(const MarginsF &l, const MarginsF &r) {
    return MarginsF(
        l.left()   / r.left(), 
        l.top()    / r.top(), 
        l.right()  / r.right(), 
        l.bottom() / r.bottom()
    );
}

QRectF SceneUtility::cwiseMul(const QRectF &l, const QSizeF &r) {
    return QRectF(
        l.left()   * r.width(),
        l.top()    * r.height(),
        l.width()  * r.width(),
        l.height() * r.height()
    );
}

QRectF SceneUtility::cwiseDiv(const QRectF &l, const QSizeF &r) {
    return QRectF(
        l.left()   / r.width(),
        l.top()    / r.height(),
        l.width()  / r.width(),
        l.height() / r.height()
    );
}

QMargins SceneUtility::cwiseSub(const QMargins &l, const QMargins &r) {
    return QMargins(
        l.left()   - r.left(), 
        l.top()    - r.top(), 
        l.right()  - r.right(), 
        l.bottom() - r.bottom()
    );
}

QMargins SceneUtility::cwiseAdd(const QMargins &l, const QMargins &r) {
    return QMargins(
        l.left()   + r.left(), 
        l.top()    + r.top(), 
        l.right()  + r.right(), 
        l.bottom() + r.bottom()
    );
}

namespace {
    template<class Rect, class Size>
    Rect resizeRectInternal(const Rect &rect, const Size &size, Qt::WindowFrameSection resizeGrip) {
        /* Note that QRect::right & QRect::bottom return not what is expected (see Qt docs).
         * This is why these methods are not used here. */
        switch (resizeGrip) {
        case Qt::LeftSection:
            return Rect(
                rect.left() + rect.width() - size.width(), 
                rect.top(),
                size.width(),
                rect.height()
            );
        case Qt::TopLeftSection:
            return Rect(
                rect.left() + rect.width() - size.width(), 
                rect.top() + rect.height() - size.height(),
                size.width(), 
                size.height()
            );
        case Qt::TopSection:
            return Rect(
                rect.left(), 
                rect.top() + rect.height() - size.height(),
                rect.width(), 
                size.height()
            );
        case Qt::TopRightSection:
            return Rect(
                rect.left(),
                rect.top() + rect.height() - size.height(),
                size.width(),
                size.height()
            );
        case Qt::RightSection:
            return Rect(
                rect.left(),
                rect.top(),
                size.width(),
                rect.height()
            );
        case Qt::BottomRightSection:
            return Rect(
                rect.left(),
                rect.top(),
                size.width(),
                size.height()
            );
        case Qt::BottomSection:
            return Rect(
                rect.left(),
                rect.top(),
                rect.width(),
                size.height()
            );
        case Qt::BottomLeftSection:
            return Rect(
                rect.left() + rect.width() - size.width(), 
                rect.top(),
                size.width(), 
                size.height()
            );
        default:
            qnWarning("Invalid resize grip '%1'.", resizeGrip);
            return rect;
        }
    }

} // anonymous namespace

QRectF SceneUtility::resizeRect(const QRectF &rect, const QSizeF &size, Qt::WindowFrameSection resizeGrip) {
    return resizeRectInternal(rect, size, resizeGrip);
}

QRect SceneUtility::resizeRect(const QRect &rect, const QSize &size, Qt::WindowFrameSection resizeGrip) {
    return resizeRectInternal(rect, size, resizeGrip);
}

QSizeF SceneUtility::sizeDelta(const MarginsF &margins) {
    return QSizeF(margins.left() + margins.right(), margins.top() + margins.bottom());
}

QSize SceneUtility::sizeDelta(const QMargins &margins) {
    return QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}

qreal SceneUtility::aspectRatio(const QSizeF &size) {
    return size.width() / size.height();
}

qreal SceneUtility::aspectRatio(const QSize &size) {
    return static_cast<qreal>(size.width()) / size.height();
}

qreal SceneUtility::aspectRatio(const QRect &rect) {
    return aspectRatio(rect.size());
}

qreal SceneUtility::aspectRatio(const QRectF &rect) {
    return aspectRatio(rect.size());
}

qreal SceneUtility::length(const QPointF &point) {
    return std::sqrt(point.x() * point.x() + point.y() * point.y());
}

qreal SceneUtility::length(const QSizeF &size) {
    return std::sqrt(size.width() * size.width() + size.height() * size.height());
}

QPointF SceneUtility::normalized(const QPointF &point) {
    return point / length(point);
}

QPointF SceneUtility::normal(const QPointF &point) {
    return QPointF(point.y(), -point.x());
}

qreal SceneUtility::atan2(const QPointF &point) {
    return std::atan2(point.y(), point.x());
}

QPointF SceneUtility::pointCentroid(const QPolygonF &polygon) {
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

QRectF SceneUtility::mapRectToScene(const QGraphicsView *view, const QRect &rect) {
    return mapRectToScene(view, QRectF(rect));
}

QRectF SceneUtility::mapRectToScene(const QGraphicsView *view, const QRectF &rect) {
    return view->viewportTransform().inverted().mapRect(rect);
}

QRect SceneUtility::mapRectFromScene(const QGraphicsView *view, const QRectF &rect) {
    return view->viewportTransform().mapRect(rect).toRect();
}

qreal SceneUtility::scaleFactor(QSizeF size, QSizeF bounds, Qt::AspectRatioMode mode) {
    if(mode == Qt::KeepAspectRatioByExpanding) {
        return qMax(bounds.width() / size.width(), bounds.height() / size.height());
    } else {
        return qMin(bounds.width() / size.width(), bounds.height() / size.height());
    }
}

QPoint SceneUtility::bounded(const QPoint &pos, const QRect &bounds) {
    return QPoint(
        qBound(bounds.left(), pos.x(), bounds.right()),
        qBound(bounds.top(), pos.y(), bounds.bottom())
    );
}

QSizeF SceneUtility::bounded(const QSizeF &size, const QSizeF &maxSize, Qt::AspectRatioMode mode) {
    if(mode == Qt::IgnoreAspectRatio)
        return size.boundedTo(maxSize);

    qreal factor = scaleFactor(size, maxSize, mode);
    if(factor >= 1.0)
        return size;

    return size * factor;
}

QSizeF SceneUtility::expanded(const QSizeF &size, const QSizeF &minSize, Qt::AspectRatioMode mode) {
    if(mode == Qt::IgnoreAspectRatio)
        return size.expandedTo(minSize);

    qreal factor = scaleFactor(size, minSize, mode);
    if(factor <= 1.0)
        return size;

    return size * factor;
}

QSizeF SceneUtility::expanded(qreal aspectRatio, const QSizeF &minSize, Qt::AspectRatioMode mode) {
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

QRectF SceneUtility::expanded(qreal aspectRatio, const QRectF &minRect, Qt::AspectRatioMode mode, Qt::Alignment alignment) {
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

QRectF SceneUtility::dilated(const QRectF &rect, const QSizeF &amount) {
    return rect.adjusted(-amount.width(), -amount.height(), amount.width(), amount.height());
}

QRectF SceneUtility::dilated(const QRectF &rect, const MarginsF &amount) {
    return rect.adjusted(-amount.left(), -amount.top(), amount.right(), amount.bottom());
}

QSizeF SceneUtility::dilated(const QSizeF &size, const MarginsF &amount) {
    return size + sizeDelta(amount);
}

QRectF SceneUtility::eroded(const QRectF &rect, const MarginsF &amount) {
    return rect.adjusted(amount.left(), amount.top(), -amount.right(), -amount.bottom());
}

QRect SceneUtility::eroded(const QRect &rect, const QMargins &amount) {
    return rect.adjusted(amount.left(), amount.top(), -amount.right(), -amount.bottom());
}

QSizeF SceneUtility::eroded(const QSizeF &size, const MarginsF &amount) {
    return size - sizeDelta(amount);
}

QSize SceneUtility::eroded(const QSize &size, const QMargins &amount) {
    return size - sizeDelta(amount);
}

bool SceneUtility::contains(const QSizeF &size, const QSizeF &otherSize) {
    return size.width() >= otherSize.width() && size.height() >= otherSize.height();
}

bool SceneUtility::contains(const QSize &size, const QSize &otherSize) {
    return size.width() >= otherSize.width() && size.height() >= otherSize.height();
}

void SceneUtility::moveViewport(QGraphicsView *view, const QPoint &viewportPositionDelta) {
    assert(view != NULL);

    moveViewportScene(view, view->mapToScene(viewportPositionDelta) - view->mapToScene(QPoint(0, 0)));
}

void SceneUtility::moveViewport(QGraphicsView *view, const QPointF &viewportPositionDelta) {
    assert(view != NULL);

    QTransform viewportToScene = view->viewportTransform().inverted();
    moveViewportScene(view, viewportToScene.map(viewportPositionDelta) - viewportToScene.map(QPointF(0.0, 0.0)));
}

void SceneUtility::moveViewportScene(QGraphicsView *view, const QPointF &scenePositionDelta) {
    assert(view != NULL);

    QGraphicsView::ViewportAnchor oldAnchor = view->transformationAnchor();
    bool oldInteractive = view->isInteractive();
    view->setInteractive(false); /* View will re-fire stored mouse event if we don't do this. */
    view->setTransformationAnchor(QGraphicsView::NoAnchor);

    view->translate(-scenePositionDelta.x(), -scenePositionDelta.y());

    view->setTransformationAnchor(oldAnchor);
    view->setInteractive(oldInteractive);

    debugViewportRect(view);
}

void SceneUtility::moveViewportSceneTo(QGraphicsView *view, const QPointF &sceneCenter) {
    assert(view != NULL);

    QTransform viewportToScene = view->viewportTransform().inverted();
    QPointF currentSceneCenter = viewportToScene.map(QRectF(view->viewport()->rect()).center());

    moveViewportScene(view, sceneCenter - currentSceneCenter);
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

void SceneUtility::scaleViewport(QGraphicsView *view, qreal factor, const QPoint &viewportAnchor) {
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
    
    debugViewportRect(view);
}

void SceneUtility::scaleViewport(QGraphicsView *view, qreal factor, QGraphicsView::ViewportAnchor anchor) {
    return scaleViewport(view, factor, anchorViewportPosition(view, anchor));
}

void SceneUtility::scaleViewportTo(QGraphicsView *view, const QSizeF &size, Qt::AspectRatioMode mode, QGraphicsView::ViewportAnchor anchor) {
    assert(view != NULL);

    qreal factor = scaleFactor(mapRectToScene(view, view->viewport()->rect()).size(), size, mode);

    scaleViewport(view, factor, anchor);
}

QGraphicsView *SceneUtility::view(QWidget *viewport) {
    assert(viewport != NULL);

    return checked_cast<QGraphicsView *>(viewport->parent());
}

