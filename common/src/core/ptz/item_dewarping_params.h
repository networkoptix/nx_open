#ifndef ITEM_DEWARPING_PARAMS_H
#define ITEM_DEWARPING_PARAMS_H

#include <QtCore/QMetaType>

#include <boost/operators.hpp>

#include <utils/math/defines.h>

// TODO: #Elric doesn't really belong in this folder
struct QnItemDewarpingParams: public boost::equality_comparable1<QnItemDewarpingParams> {
    /** Whether dewarping is currently enabled. */
    bool enabled;

    /** Pan in radians. */
    qreal xAngle;

    /** Tilt in radians. */
    qreal yAngle;

    /** Fov in radians. */
    qreal fov;

    /** Aspect ratio correction? */ //TODO: #Elric
    qreal panoFactor;

    QnItemDewarpingParams(): enabled(false), xAngle(0.0), yAngle(0.0), fov(70.0 * M_PI / 180.0), panoFactor(1.0) {}

    friend bool operator==(const QnItemDewarpingParams &l, const QnItemDewarpingParams &r);
};

Q_DECLARE_METATYPE(QnItemDewarpingParams)
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QnItemDewarpingParams)

#endif // ITEM_DEWARPING_PARAMS_H
