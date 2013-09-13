#ifndef QN_PTZ_LIMITS_H
#define QN_PTZ_LIMITS_H

#include <QtCore/QtGlobal>

struct QnPtzLimits {
    qreal minPan;
    qreal maxPan;
    qreal minTilt;
    qreal maxTilt;
    qreal minFov;
    qreal maxFov;
};

#endif // QN_PTZ_LIMITS_H
