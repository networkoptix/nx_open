#ifndef __FISHEYE_COMMON_H__
#define __FISHEYE_COMMON_H__

#include <QObject>

struct DevorpingParams
{
    DevorpingParams(): enabled(false), xAngle(0.0), yAngle(0.0), fov(M_PI/2.0), pAngle(0.0), aspectRatio(1.0) {}

    bool enabled;
    // view angle and FOV at radians
    qreal xAngle;
    qreal yAngle;
    qreal fov;
    // perspective correction angle
    qreal pAngle;
    qreal aspectRatio;
};

#endif // __FISHEYE_COMMON_H__
