#include "fixed_rotation.h"

#include <QtCore/QtMath>
#include <QtWidgets/QGraphicsWidget>

#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>

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
        disconnect(m_target.data(), NULL, this, NULL);

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

void QnFixedRotationTransform::setAngle(nx::vms::client::desktop::Rotation angle)
{
    setAngle(-1.0 * angle.value());
}

void QnFixedRotationTransform::updateOrigin() {
    if(!target())
        return;

    QPointF origin;

    auto standardRotation = nx::vms::client::desktop::Rotation::closestStandardRotation(angle());

    if (qFuzzyEquals(standardRotation.value(), 0))
    {
        origin = QPointF(0.0, 0.0);
    }
    else if (qFuzzyEquals(standardRotation.value(), 90))
    {
        origin = target()->rect().center();
        origin.setX(origin.y());
    }
    else if (qFuzzyEquals(standardRotation.value(), 180))
    {
        origin = target()->rect().center();
    }
    else if (qFuzzyEquals(standardRotation.value(), 270))
    {
        origin = target()->rect().center();
        origin.setY(origin.x());
    }
    else
    {
        NX_ASSERT(false, "Standard rotation is not handled");
        return;
    }

    setOrigin(QVector3D(origin));
}
