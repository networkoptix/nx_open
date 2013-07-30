#ifndef QN_DEWARPING_PARAMS_H
#define QN_DEWARPING_PARAMS_H

#include <boost/operators.hpp>

class DewarpingParams: public boost::equality_comparable1<DewarpingParams> {
public:
    DewarpingParams(): enabled(false), horizontalView(false), xAngle(0.0), yAngle(0.0), fov(M_PI/2.0), fovRot(0.0), panoFactor(1.0) {}
    
    DewarpingParams(const DewarpingParams &other) {
        enabled = other.enabled;
        horizontalView = other.horizontalView;
        xAngle = other.xAngle;
        yAngle = other.yAngle;
        fov = other.fov;
        fovRot = other.fovRot;
        panoFactor = other.panoFactor;
    }

    friend bool operator==(const DewarpingParams &l, const DewarpingParams &r);

    QByteArray serialize() const;
    static DewarpingParams deserialize(const QByteArray &data);

public:
    bool enabled;
    bool horizontalView;
    // view angle and FOV at radians
    qreal xAngle;
    qreal yAngle;
    qreal fov;
    // perspective correction angle
    qreal fovRot;
    qreal panoFactor;
};

Q_DECLARE_METATYPE(DewarpingParams);

#endif // QN_DEWARPING_PARAMS_H
