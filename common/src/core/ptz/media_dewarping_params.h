#ifndef MEDIA_DEWARPING_PARAMS_H
#define MEDIA_DEWARPING_PARAMS_H

#include <QtCore/QMetaType>

#include <boost/operators.hpp>

#include <utils/common/json_fwd.h>

struct QnMediaDewarpingParams: public boost::equality_comparable1<QnMediaDewarpingParams> {
public:
    enum ViewMode {
        Horizontal,
        VerticalDown,
        VerticalUp
    };

    QnMediaDewarpingParams(): enabled(false), viewMode(VerticalDown), fovRot(0.0) {}

    QnMediaDewarpingParams(const QnMediaDewarpingParams &other) {
        enabled = other.enabled;
        viewMode = other.viewMode;
        fovRot = other.fovRot;
    }

    friend bool operator==(const QnMediaDewarpingParams &l, const QnMediaDewarpingParams &r);

    /**
     * Compatibility function to support reading data from previous software version.
     */
    static QnMediaDewarpingParams deserialized(const QByteArray &data);
public:
    /** Whether dewarping is currently enabled. */
    bool enabled;

    /** Camera mounting mode. */
    ViewMode viewMode;

    /** View correction angle, in radians. */
    qreal fovRot;
};

Q_DECLARE_METATYPE(QnMediaDewarpingParams)
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QnMediaDewarpingParams)

#endif // MEDIA_DEWARPING_PARAMS_H
