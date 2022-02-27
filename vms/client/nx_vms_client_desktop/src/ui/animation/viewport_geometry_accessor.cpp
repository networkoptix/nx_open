// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "viewport_geometry_accessor.h"

#include <QtWidgets/QGraphicsView>
#include <QtCore/QVariant>

#include <utils/common/checked_cast.h>
#include <ui/common/scene_transformations.h>

QVariant ViewportGeometryAccessor::get(const QObject* object) const
{
    const auto view = checked_cast<const QGraphicsView*>(object);
    return QnSceneTransformations::mapRectToScene(view, view->viewport()->rect());
}

void ViewportGeometryAccessor::set(QObject* object, const QVariant& value) const
{
    auto view = checked_cast<QGraphicsView*>(object);
    QRectF sceneRect = value.toRectF();

    QnSceneTransformations::moveViewportSceneTo(view, sceneRect.center());
    QnSceneTransformations::scaleViewportTo(view, sceneRect.size(), Qt::KeepAspectRatioByExpanding);
}
