#ifndef QN_PTZ_LIMITS_H
#define QN_PTZ_LIMITS_H

#include <QtCore/QtGlobal>
#include <QtGui/QVector3D>

struct QnPtzLimits {
    QnPtzLimits(): minPan(0), maxPan(360), minTilt(-90), maxTilt(90), minFov(0), maxFov(360) {}

    qreal minPan;
    qreal maxPan;
    qreal minTilt;
    qreal maxTilt;
    qreal minFov;
    qreal maxFov;
};


inline QVector3D qBound(const QVector3D &position, const QnPtzLimits &limits) {
    return QVector3D(
        qBound<float>(limits.minPan,  position.x(), limits.maxPan),
        qBound<float>(limits.minTilt, position.y(), limits.maxTilt),
        qBound<float>(limits.minFov,  position.z(), limits.maxFov)
    );
}

#endif // QN_PTZ_LIMITS_H
