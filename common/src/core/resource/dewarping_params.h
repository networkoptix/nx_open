#ifndef QN_DEWARPING_PARAMS_H
#define QN_DEWARPING_PARAMS_H

#include <QtCore/QByteArray>
#include <QtCore/QtGlobal>
#include <QtCore/QMetaType>
#include <QtCore/QList>

#include <utils/math/math.h>

#include <boost/operators.hpp>

struct DewarpingParams: public boost::equality_comparable1<DewarpingParams> {
public:
    enum ViewMode {
        Horizontal,
        VerticalDown,
        VerticalUp
    };

    DewarpingParams(): enabled(false), viewMode(VerticalDown), xAngle(0.0), yAngle(0.0), fov(70.0 * M_PI / 180.0), fovRot(0.0), panoFactor(1.0) {}
    
    DewarpingParams(const DewarpingParams &other) {
        enabled = other.enabled;
        viewMode = other.viewMode;
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
    /** Whether dewarping is currently enabled. */
    bool enabled;

    /** Camera mounting mode. */
    ViewMode viewMode;
    
    /** Pan in radians. */
    qreal xAngle;

    /** Tilt in radians. */
    qreal yAngle;

    /** Fov in radians. */
    qreal fov;

    /** View correction angle, in radians. */
    qreal fovRot;

    /** Aspect ratio correction? */ // TODO
    qreal panoFactor;
};

Q_DECLARE_METATYPE(DewarpingParams);

#endif // QN_DEWARPING_PARAMS_H
