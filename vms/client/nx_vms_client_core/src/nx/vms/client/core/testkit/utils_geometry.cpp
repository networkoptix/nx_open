// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <QtGui/QWindow>
#include <QtQuick/QQuickItem>

namespace nx::vms::client::core::testkit::utils {

QRect globalRect(QVariant object)
{
    // If we already got the rectangle, just pass it through.
    const QMetaType::Type objectType = (QMetaType::Type)object.typeId();
    if (objectType == QMetaType::QRect || objectType == QMetaType::QRectF)
        return object.toRect();

    auto qobj = qvariant_cast<QObject*>(object);

    // Map object rectangle to global, each type has it's own way of mapping the coordinates.
    if (auto qi = qobject_cast<QQuickItem*>(qobj))
    {
        const QPointF topLeft = qi->mapToGlobal(qi->boundingRect().topLeft());
        const QPointF bottomRight = qi->mapToGlobal(qi->boundingRect().bottomRight());
        return QRect(topLeft.toPoint(), bottomRight.toPoint());
    }
    else if (auto w = qobject_cast<QWindow*>(qobj))
    {
        const QPointF topLeft = w->mapToGlobal(QPoint(0, 0));
        const QPointF bottomRight = w->mapToGlobal(QPoint(w->width(), w->height()));
        return QRect(topLeft.toPoint(), bottomRight.toPoint());
    }
    return QRect();
}

} // namespace nx::vms::client::core::testkit::utils
