#include "fixed_rotation.h"

#include <QtGui/QGraphicsWidget>

Qn::FixedRotation fixedRotationFromDegrees(qreal degrees){
    int angle = qRound(degrees);

    //
    // TODO: #GDM 
    // The logic here seems overly complicated to me.
    // Much simpler solution would be:
    // 
    // qreal result = std::fmod(degrees + 45, 360);
    // if(result < 0)
    //     result += 360;
    //     
    // return static_cast<Qn::FixedRotation>(qFloor(result / 90) * 90);
    // 

    // Limiting angle to (-180; 180]
    while (angle <= -180)
        angle += 360;
    while (angle > 180)
        angle -= 360;


    // (-45; 45]
    if (angle > -45 && angle <= 45)
        return Qn::Angle0;

    // (-180; -135] U (135; 180]
    if (angle > 135 || angle <= -135)
        return Qn::Angle180;

    // (45; 135]
    else if (angle > 0)
        return Qn::Angle90;

    // (-135; -45]
    return Qn::Angle270;
}


QnFixedRotationTransform::QnFixedRotationTransform(QObject *parent): 
    base_type(parent) 
{
    connect(this, SIGNAL(angleChanged()), this, SLOT(updateOrigin()));
}
    
QGraphicsWidget *QnFixedRotationTransform::target() {
    return m_target.data();
}

void QnFixedRotationTransform::setTarget(QGraphicsWidget *target) {
    if(m_target) {
        disconnect(m_target.data(), NULL, this, NULL);

        QList<QGraphicsTransform *> transformations = m_target.data()->transformations();
        transformations.removeAll(this);
        m_target.data()->setTransformations(transformations);
    }

    m_target = target;

    if(m_target) {
        connect(m_target.data(), SIGNAL(geometryChanged()), this, SLOT(updateOrigin()));

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
