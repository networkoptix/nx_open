// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fixed_rotation.h"

#include <QtCore/QtMath>
#include <QtWidgets/QGraphicsWidget>

#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>

using Rotation = nx::vms::client::core::Rotation;

QnFixedRotationTransform::QnFixedRotationTransform(QObject *parent):
    base_type(parent)
{
    connect(this, &QGraphicsRotation::angleChanged, this, &QnFixedRotationTransform::updateOrigin);
}

QGraphicsWidget *QnFixedRotationTransform::target() {
    return m_target.data();
}

void QnFixedRotationTransform::setTarget(QGraphicsWidget *target)
{
    if (m_target)
    {
        disconnect(m_target.data(), nullptr, this, nullptr);

        QList<QGraphicsTransform *> transformations = m_target.data()->transformations();
        transformations.removeAll(this);
        m_target.data()->setTransformations(transformations);
    }

    m_target = target;

    if (m_target)
    {
        connect(target, &QGraphicsWidget::geometryChanged, this, &QnFixedRotationTransform::updateOrigin);

        QList<QGraphicsTransform *> transformations = m_target.data()->transformations();
        transformations.push_back(this);
        m_target.data()->setTransformations(transformations);
    }

    updateOrigin();
}

void QnFixedRotationTransform::setAngle(Rotation angle)
{
    setAngle(-1.0 * angle.value());
}

void QnFixedRotationTransform::updateOrigin() {
    if(!target())
        return;

    QPointF origin;

    using StandardRotation = nx::vms::client::core::StandardRotation;
    switch (Rotation::standardRotation(angle()))
    {
        case StandardRotation::rotate0:
            origin = QPointF(0.0, 0.0);
            break;
        case StandardRotation::rotate90:
            origin = target()->rect().center();
            origin.setX(origin.y());
            break;
        case StandardRotation::rotate180:
            origin = target()->rect().center();
            break;
        case StandardRotation::rotate270:
            origin = target()->rect().center();
            origin.setY(origin.x());
            break;
        default:
            NX_ASSERT(false, "Standard rotation is not handled");
            break;
    }
    setOrigin(QVector3D(origin));
}
