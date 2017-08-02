#ifndef ITEM_DEWARPING_PARAMS_H
#define ITEM_DEWARPING_PARAMS_H

#include <cmath>
#include <QtCore/QMetaType>

#ifndef Q_MOC_RUN
#include <boost/operators.hpp>
#endif

#include <nx/fusion/model_functions_fwd.h>

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

    /** Aspect ratio correction?  // TODO: #Elric
     * multiplier for 90 degrees of.. // TODO: #vasilenko
     */
    int panoFactor;

    QnItemDewarpingParams();

    friend bool operator==(const QnItemDewarpingParams &l, const QnItemDewarpingParams &r);
};

#define QnItemDewarpingParams_Fields (enabled)(xAngle)(yAngle)(fov)(panoFactor)

QN_FUSION_DECLARE_FUNCTIONS(QnItemDewarpingParams, (json)(metatype))

#endif // ITEM_DEWARPING_PARAMS_H
