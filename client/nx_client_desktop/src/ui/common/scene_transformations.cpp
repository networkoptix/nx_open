#include "scene_transformations.h"

#include <nx/client/core/utils/geometry.h>
#include <nx/utils/log/assert.h>

using nx::client::core::Geometry;

namespace {

#if defined(QN_SCENE_TRANSFORMATIONS_DEBUG)

void debugViewportRect(QGraphicsView* view)
{
    QRectF rect = Geometry::mapRectToScene(view, view->viewport()->rect());
    qDebug() << "SCENE VIEWPORT RECT:" << rect;
}

#else

#define debugViewportRect(...)

#endif

} // namespace

void QnSceneTransformations::moveViewport(QGraphicsView* view, const QPoint& viewportPositionDelta)
{
    NX_ASSERT(view);

    moveViewportScene(
        view, view->mapToScene(viewportPositionDelta) - view->mapToScene(QPoint(0, 0)));
}

void QnSceneTransformations::moveViewport(
    QGraphicsView* view, const QPointF& viewportPositionDelta)
{
    NX_ASSERT(view);

    QTransform viewportToScene = view->viewportTransform().inverted();
    moveViewportScene(
        view, viewportToScene.map(viewportPositionDelta) - viewportToScene.map(QPointF(0.0, 0.0)));
}

void QnSceneTransformations::moveViewportScene(
    QGraphicsView* view, const QPointF& scenePositionDelta)
{
    NX_ASSERT(view);

    QGraphicsView::ViewportAnchor oldAnchor = view->transformationAnchor();
    bool oldInteractive = view->isInteractive();
    view->setInteractive(false); /* View will re-fire stored mouse event if we don't do this. */
    view->setTransformationAnchor(QGraphicsView::NoAnchor);

    view->translate(-scenePositionDelta.x(), -scenePositionDelta.y());

    view->setTransformationAnchor(oldAnchor);
    view->setInteractive(oldInteractive);

    debugViewportRect(view);
}

void QnSceneTransformations::moveViewportSceneTo(QGraphicsView* view, const QPointF& sceneCenter)
{
    NX_ASSERT(view);

    QTransform viewportToScene = view->viewportTransform().inverted();
    QPointF currentSceneCenter = viewportToScene.map(QRectF(view->viewport()->rect()).center());

    moveViewportScene(view, sceneCenter - currentSceneCenter);
}

namespace {
QPoint anchorViewportPosition(QGraphicsView* view, QGraphicsView::ViewportAnchor anchor)
{
    switch (anchor)
    {
        case QGraphicsView::AnchorViewCenter: return view->viewport()->rect().center();
        case QGraphicsView::AnchorUnderMouse: return view->mapFromGlobal(QCursor::pos());
        default: return QPoint(0, 0);
    }
}
} // namespace

void QnSceneTransformations::scaleViewport(
    QGraphicsView* view, qreal factor, const QPoint& viewportAnchor)
{
    NX_ASSERT(view);

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

void QnSceneTransformations::scaleViewport(
    QGraphicsView* view, qreal factor, QGraphicsView::ViewportAnchor anchor)
{
    return scaleViewport(view, factor, anchorViewportPosition(view, anchor));
}

void QnSceneTransformations::scaleViewportTo(QGraphicsView* view,
    const QSizeF& size,
    Qt::AspectRatioMode mode,
    QGraphicsView::ViewportAnchor anchor)
{
    NX_ASSERT(view);

    qreal factor =
        Geometry::scaleFactor(mapRectToScene(view, view->viewport()->rect()).size(), size, mode);

    scaleViewport(view, factor, anchor);
}

QRectF QnSceneTransformations::mapRectToScene(const QGraphicsView* view, const QRect& rect)
{
    return mapRectToScene(view, QRectF(rect));
}

QRectF QnSceneTransformations::mapRectToScene(const QGraphicsView* view, const QRectF& rect)
{
    return view->viewportTransform().inverted().mapRect(rect);
}

QRect QnSceneTransformations::mapRectFromScene(const QGraphicsView* view, const QRectF& rect)
{
    return view->viewportTransform().mapRect(rect).toRect();
}
