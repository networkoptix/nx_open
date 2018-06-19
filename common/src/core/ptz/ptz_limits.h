#pragma once

#include <QtCore/QtGlobal>
#include <QtCore/QMetaType>
#include <QtGui/QVector3D>

#include <utils/math/math.h>

#include <nx/core/ptz/vector.h>

struct QnPtzLimits
{
    qreal minPan = 0;
    qreal maxPan = 360;
    qreal minTilt = -90;
    qreal maxTilt = 90;
    qreal minFov = 0;
    qreal maxFov = 360;
    qreal minRotation = 0;
    qreal maxRotation = 360;
    int maxPresetNumber = 0; //< -1 means unlimited

    qreal minPanSpeed = -1.0;
    qreal maxPanSpeed = 1.0;
    qreal minTiltSpeed = -1.0;
    qreal maxTiltSpeed = 1.0;
    qreal minZoomSpeed = -1.0;
    qreal maxZoomSpeed = 1.0;
    qreal minRotationSpeed = -1.0;
    qreal maxRotationSpeed = -1.0;
};
#define QnPtzLimits_Fields (minPan)(maxPan)(minTilt)(maxTilt)(minFov)(maxFov)\
        (minRotation)(maxRotation)(maxPresetNumber)\
        (minPanSpeed)(maxPanSpeed)(minTiltSpeed)(maxTiltSpeed)(minZoomSpeed)(maxZoomSpeed)\
        (minRotationSpeed)(maxRotationSpeed)

Q_DECLARE_TYPEINFO(QnPtzLimits, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(QnPtzLimits);

#if 0
// TODO: #dmishin remove it.
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
#endif
inline nx::core::ptz::Vector qBound(
    const nx::core::ptz::Vector& position,
    const QnPtzLimits& limits)
{
    bool unlimitedPan = false;
    const qreal panRange = (limits.maxPan - limits.minPan);
    if (qFuzzyCompare(panRange, 360) || panRange > 360)
        unlimitedPan = true;

    qreal pan = position.pan;
    if (!unlimitedPan && !qBetween(limits.minPan, pan, limits.maxPan))
    {
        /* Round it to the nearest boundary. */
        qreal panBase = limits.minPan - qMod(limits.minPan, 360.0);
        qreal panShift = qMod(pan, 360.0);

        qreal bestPan = pan;
        qreal bestDist = std::numeric_limits<qreal>::max();

        pan = panBase - 360.0 + panShift;
        for (int i = 0; i < 3; i++)
        {
            qreal dist;
            if (pan < limits.minPan)
                dist = limits.minPan - pan;
            else if (pan > limits.maxPan)
                dist = pan - limits.maxPan;
            else
                dist = 0.0;

            if (dist < bestDist)
            {
                bestDist = dist;
                bestPan = pan;
            }

            pan += 360.0;
        }

        pan = bestPan;
    }

    return nx::core::ptz::Vector(
        pan,
        qBound<float>(limits.minTilt, position.tilt, limits.maxTilt),
        qBound<float>(limits.minRotation, position.rotation, limits.maxRotation),
        qBound<float>(limits.minFov, position.zoom, limits.maxFov));
}

