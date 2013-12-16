#ifndef QN_PTZ_TOUR_H
#define QN_PTZ_TOUR_H

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMetaType>

#include <boost/operators.hpp>

#include "ptz_fwd.h"

struct QnPtzTourSpot: public boost::equality_comparable1<QnPtzTourSpot> {
    QnPtzTourSpot(): stayTime(0), speed(1.0) {}
    QnPtzTourSpot(const QString &presetId, qint64 stayTime, qreal speed): presetId(presetId), stayTime(stayTime), speed(speed) {}

    QString presetId;
    qint64 stayTime;
    qreal speed;

    friend bool operator==(const QnPtzTourSpot &l, const QnPtzTourSpot &r);
};


struct QnPtzTour: public boost::equality_comparable1<QnPtzTour> {
    QnPtzTour() {}
    QnPtzTour(const QString &id, const QString &name, const QnPtzTourSpotList &spots): id(id), name(name), spots(spots) {}

    QString id;
    QString name;
    QnPtzTourSpotList spots;

    friend bool operator==(const QnPtzTour &l, const QnPtzTour &r);
};

Q_DECLARE_METATYPE(QnPtzTourSpot)
Q_DECLARE_METATYPE(QnPtzTour)
Q_DECLARE_METATYPE(QnPtzTourList)

#endif // QN_PTZ_TOUR_H
