#ifndef MEDIA_DEWARPING_PARAMS_H
#define MEDIA_DEWARPING_PARAMS_H

#include <QtCore/QMetaType>

#include <boost/operators.hpp>

#include <utils/common/json_fwd.h>

// TODO: #Elric doesn't really belong in this folder
struct QnMediaDewarpingParams: public boost::equality_comparable1<QnMediaDewarpingParams> {
public:
    enum ViewMode {
        Horizontal,
        VerticalDown,
        VerticalUp
    };

    QnMediaDewarpingParams(): enabled(false), viewMode(VerticalDown), fovRot(0.0), xCenter(0.5), yCenter(0.5), radius(0.5) {}

    QnMediaDewarpingParams(const QnMediaDewarpingParams &other) {
        enabled = other.enabled;
        viewMode = other.viewMode;
        fovRot = other.fovRot;
        xCenter = other.xCenter;
        yCenter = other.yCenter;
        radius = other.radius;
    }

    friend bool operator==(const QnMediaDewarpingParams &l, const QnMediaDewarpingParams &r);
    friend QDebug &operator<<(QDebug &stream, const QnMediaDewarpingParams &params);

    /**
     * Compatibility function to support reading data from previous software version.
     */
    static QnMediaDewarpingParams deserialized(const QByteArray &data);

    /** List of all possible panoFactor values for the resource with this dewarping parameters. */
    const QList<int>& allowedPanoFactorValues() const;

    /** List of all possible panoFactor values for the selected view mode. */
    static const QList<int>& allowedPanoFactorValues(ViewMode mode);
public:
    /** Whether dewarping is currently enabled. */
    bool enabled;

    /** Camera mounting mode. */
    ViewMode viewMode;

    /** View correction angle, in radians. */
    qreal fovRot;

    /** Normalized position of the circle*/ 
    qreal xCenter;
    qreal yCenter;

    /** Circle radius in range 0..1 (r/width) */ 
    qreal radius;
};

Q_DECLARE_METATYPE(QnMediaDewarpingParams)
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QnMediaDewarpingParams)

#endif // MEDIA_DEWARPING_PARAMS_H
