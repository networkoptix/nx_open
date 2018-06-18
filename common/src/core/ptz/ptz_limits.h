#pragma once

#include <QtCore/QtGlobal>
#include <QtCore/QMetaType>
#include <QtGui/QVector3D>

#include <utils/math/math.h>

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
