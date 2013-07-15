#ifndef __FISHEYE_COMMON_H__
#define __FISHEYE_COMMON_H__

#include <QObject>

struct DevorpingParams
{
    DevorpingParams(): enabled(false), horizontalView(false), xAngle(0.0), yAngle(0.0), fov(M_PI/2.0), fovRot(0.0), aspectRatio(1.0) {}
    bool operator==(const DevorpingParams& other) const
    {
        if (enabled != other.enabled)
            return false;
        if (horizontalView != other.horizontalView)
            return false;
        if (fabs(xAngle - other.xAngle) > 0.0001)
            return false;
        if (fabs(yAngle - other.yAngle) > 0.0001)
            return false;
        if (fabs(fovRot - other.fovRot) > 0.0001)
            return false;
        if (fabs(fov - other.fov) > 0.0001)
            return false;
        if (fabs(aspectRatio - other.aspectRatio) > 0.0001)
            return false;

        return true;
    }

    DevorpingParams(const DevorpingParams& other)
    {
        enabled = other.enabled;
        horizontalView = other.horizontalView;
        xAngle = other.xAngle;
        yAngle = other.yAngle;
        fov = other.fov;
        fovRot = other.fovRot;
        aspectRatio = other.aspectRatio;
    }

    static const char DELIM = ';';

    QByteArray serialize() const
    {
        QByteArray rez;
        rez.append(enabled ? '1' : '0').append(DELIM);
        rez.append(horizontalView ? '1' : '0').append(DELIM);
        rez.append(QByteArray::number(xAngle)).append(DELIM);
        rez.append(QByteArray::number(xAngle)).append(DELIM);
        rez.append(QByteArray::number(fov)).append(DELIM);
        rez.append(QByteArray::number(fovRot)).append(DELIM);
        return rez;
    }

    static DevorpingParams deserialize(const QByteArray& data)
    {
        QList<QByteArray> params = data.split(DELIM);
        DevorpingParams result;
        if (params.size() >= 6) {
            result.enabled = params[0].toInt() > 0;
            result.horizontalView = params[1].toInt() > 0;
            result.xAngle = params[2].toDouble();
            result.yAngle = params[3].toDouble();
            result.fov = params[4].toDouble();
            result.fovRot = params[5].toDouble();
        }
        return result;
    }

    bool enabled;
    bool horizontalView;
    // view angle and FOV at radians
    qreal xAngle;
    qreal yAngle;
    qreal fov;
    // perspective correction angle
    qreal fovRot;
    qreal aspectRatio;
};

Q_DECLARE_METATYPE(DevorpingParams);

#endif // __FISHEYE_COMMON_H__
