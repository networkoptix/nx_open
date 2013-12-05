#ifndef QN_PTZ_LIMITS_H
#define QN_PTZ_LIMITS_H

#include <QtCore/QtGlobal>

struct QnPtzLimits {
    QnPtzLimits(): minPan(0), maxPan(360), minTilt(-90), maxTilt(90), minFov(0), maxFov(360) {}

    qreal minPan;
    qreal maxPan;
    qreal minTilt;
    qreal maxTilt;
    qreal minFov;
    qreal maxFov;
};

#endif // QN_PTZ_LIMITS_H
