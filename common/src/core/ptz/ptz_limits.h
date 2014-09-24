#ifndef QN_PTZ_LIMITS_H
#define QN_PTZ_LIMITS_H

#include <boost/operators.hpp>

#include <QtCore/QtGlobal>
#include <QtCore/QMetaType>
#include <QtGui/QVector3D>

#include <utils/math/math.h>

struct QnPtzLimits: public boost::equality_comparable1<QnPtzLimits> {
    QnPtzLimits(): minPan(0), maxPan(360), minTilt(-90), maxTilt(90), minFov(0), maxFov(360) {}

    friend bool operator==(const QnPtzLimits &l, const QnPtzLimits &r);

    qreal minPan;
    qreal maxPan;
    qreal minTilt;
    qreal maxTilt;
    qreal minFov;
    qreal maxFov;
};
#define QnPtzLimits_Fields (minPan)(maxPan)(minTilt)(maxTilt)(minFov)(maxFov)

Q_DECLARE_TYPEINFO(QnPtzLimits, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(QnPtzLimits);


inline QVector3D qBound(const QVector3D &position, const QnPtzLimits &limits) {
    bool unlimitedPan = false;
    qreal panRange = (limits.maxPan - limits.minPan);
    if(qFuzzyCompare(panRange, 360) || panRange > 360)
        unlimitedPan = true;

    qreal pan = position.x();
    if(!unlimitedPan && !qBetween(limits.minPan, pan, limits.maxPan)) {
        /* Round it to the nearest boundary. */
        qreal panBase = limits.minPan - qMod(limits.minPan, 360.0);
        qreal panShift = qMod(pan, 360.0);

        qreal bestPan = pan;
        qreal bestDist = std::numeric_limits<qreal>::max();

        pan = panBase - 360.0 + panShift;
        for(int i = 0; i < 3; i++) {
            qreal dist;
            if(pan < limits.minPan) {
                dist = limits.minPan - pan;
            } else if(pan > limits.maxPan) {
                dist = pan - limits.maxPan;
            } else {
                dist = 0.0;
            }

            if(dist < bestDist) {
                bestDist = dist;
                bestPan = pan;
            }

            pan += 360.0;
        }

        pan = bestPan;
    }

    return QVector3D(
        pan,
        qBound<float>(limits.minTilt, position.y(), limits.maxTilt),
        qBound<float>(limits.minFov,  position.z(), limits.maxFov)
    );
}

#endif // QN_PTZ_LIMITS_H
