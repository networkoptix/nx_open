#include "fixed_rotation.h"

Qn::FixedRotation fixedRotationFromDegrees(qreal degrees){
    int angle = qRound(degrees);

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
