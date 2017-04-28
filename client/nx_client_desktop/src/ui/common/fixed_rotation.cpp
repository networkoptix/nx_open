#include "fixed_rotation.h"

#include <cmath>

#include <QtCore/QtMath>

#include <QtWidgets/QGraphicsWidget>

Qn::FixedRotation fixedRotationFromDegrees(qreal degrees){
    qreal result = std::fmod(degrees + 45, 360);
    if(result < 0)
        result += 360;
    return static_cast<Qn::FixedRotation>(qFloor(result / 90) * 90);
}

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

void QnFixedRotationTransform::setAngle(Qn::FixedRotation angle) {
    setAngle(-1.0 * static_cast<qreal>(angle));
}

void QnFixedRotationTransform::updateOrigin() {
    if(!target())
        return;

    QPointF origin;
    switch(fixedRotationFromDegrees(angle())) {
    case Qn::Angle0:
        origin = QPointF(0.0, 0.0);
        break;
    case Qn::Angle90:
        origin = target()->rect().center();
        origin.setX(origin.y());
        break;
    case Qn::Angle180:
        origin = target()->rect().center();
        break;
    case Qn::Angle270:
        origin = target()->rect().center();
        origin.setY(origin.x());
        break;
    default:
        return;
    }

    setOrigin(QVector3D(origin));
    //setOrigin(QVector3D(target()->rect().center()));
}
