#ifndef ITEM_DEWARPING_PARAMS_H
#define ITEM_DEWARPING_PARAMS_H

#include <QtCore/QMetaType>

#include <boost/operators.hpp>

#include <utils/common/model_functions_fwd.h>
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

    /** Aspect ratio correction?  //TODO: #Elric
     * multiplier for 90 degrees of.. //TODO: #vasilenko
     */
    int panoFactor;

    QnItemDewarpingParams(): enabled(false), xAngle(0.0), yAngle(0.0), fov(70.0 * M_PI / 180.0), panoFactor(1) {}

    friend bool operator==(const QnItemDewarpingParams &l, const QnItemDewarpingParams &r);
};

QN_FUSION_DECLARE_FUNCTIONS(QnItemDewarpingParams, (json)(metatype))

#endif // ITEM_DEWARPING_PARAMS_H
