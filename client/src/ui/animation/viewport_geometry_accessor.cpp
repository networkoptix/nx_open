#include "viewport_geometry_accessor.h"
#include <QGraphicsView>
#include <QVariant>
#include <utils/common/checked_cast.h>
#include <ui/common/geometry.h>

QVariant ViewportGeometryAccessor::get(const QObject *object) const {
    const QGraphicsView *view = checked_cast<const QGraphicsView *>(object);
    
    return QnGeometry::mapRectToScene(view, view->viewport()->rect());
}

void ViewportGeometryAccessor::set(QObject *object, const QVariant &value) const {
    QGraphicsView *view = checked_cast<QGraphicsView *>(object);
    QRectF sceneRect = value.toRectF();
    
    QnGeometry::moveViewportSceneTo(view, sceneRect.center());
    QnGeometry::scaleViewportTo(view, sceneRect.size(), Qt::KeepAspectRatioByExpanding);
}

