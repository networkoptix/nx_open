#include "viewport_geometry_accessor.h"
#include <QGraphicsView>
#include <QVariant>
#include <utils/common/checked_cast.h>
#include <ui/common/scene_utility.h>

QVariant ViewportGeometryAccessor::get(const QObject *object) const {
    const QGraphicsView *view = checked_cast<const QGraphicsView *>(object);
    
    return SceneUtility::mapRectToScene(view, view->viewport()->rect());
}

void ViewportGeometryAccessor::set(QObject *object, const QVariant &value) const {
    QGraphicsView *view = checked_cast<QGraphicsView *>(object);
    QRectF sceneRect = value.toRectF();
    
    SceneUtility::moveViewportSceneTo(view, sceneRect.center());
    SceneUtility::scaleViewportTo(view, sceneRect.size(), Qt::KeepAspectRatioByExpanding);
}

