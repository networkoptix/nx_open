#ifndef __FISHEYE_COMMON_H__
#define __FISHEYE_COMMON_H__

#include <QObject>

struct DevorpingParams
{
    DevorpingParams(): enabled(false), xAngle(0.0), yAngle(0.0), fov(M_PI/2.0), pAngle(0.0), aspectRatio(1.0) {}
    bool operator==(const DevorpingParams& other) const
    {
        if (enabled != other.enabled)
            return false;
        if (fabs(xAngle - other.xAngle) > 0.0001)
            return false;
        if (fabs(yAngle - other.yAngle) > 0.0001)
            return false;
        if (fabs(pAngle - other.pAngle) > 0.0001)
            return false;
        if (fabs(fov - other.fov) > 0.0001)
            return false;
        if (fabs(aspectRatio - other.aspectRatio) > 0.0001)
            return false;

        return true;
    }

    bool enabled;
    // view angle and FOV at radians
    qreal xAngle;
    qreal yAngle;
    qreal fov;
    // perspective correction angle
    qreal pAngle;
    qreal aspectRatio;
};

Q_DECLARE_METATYPE(DevorpingParams);

#endif // __FISHEYE_COMMON_H__
