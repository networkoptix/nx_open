#ifndef QN_PTZ_LIMITS_H
#define QN_PTZ_LIMITS_H

#include <boost/operators.hpp>

#include <QtCore/QtGlobal>
#include <QtCore/QMetaType>
#include <QtGui/QVector3D>

struct QnPtzLimits: public boost::equality_comparable1<QnPtzLimits> {
    QnPtzLimits(): minPan(0), maxPan(360), minTilt(-90), maxTilt(90), minFov(0), maxFov(360) {}

    friend bool operator==(const QnPtzLimits &l, const QnPtzLimits &r);

    qreal minPan;
    qreal maxPan;
    qreal minTilt;
    qreal maxTilt;
    qreal minFov;
    qreal maxFov;
};

Q_DECLARE_TYPEINFO(QnPtzLimits, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(QnPtzLimits);


inline QVector3D qBound(const QVector3D &position, const QnPtzLimits &limits) {
    bool unlimitedPan = false;
    qreal panRange = (limits.maxPan - limits.minPan);
    if(qFuzzyCompare(panRange, 360) || panRange > 360)
        unlimitedPan = true;

    return QVector3D(
        unlimitedPan ? position.x() : 
        qBound<float>(limits.minPan,  position.x(), limits.maxPan),
        qBound<float>(limits.minTilt, position.y(), limits.maxTilt),
        qBound<float>(limits.minFov,  position.z(), limits.maxFov)
    );
}

#endif // QN_PTZ_LIMITS_H
