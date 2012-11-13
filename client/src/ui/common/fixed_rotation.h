#ifndef FIXED_ROTATION_H
#define FIXED_ROTATION_H

namespace Qn {
    enum FixedRotation {
        Angle0 = 0,
        Angle90 = 90,
        Angle180 = 180,
        Angle270 = 270
    };
}

Qn::FixedRotation fixedRotationFromDegrees(qreal degrees);

#endif // FIXED_ROTATION_H
