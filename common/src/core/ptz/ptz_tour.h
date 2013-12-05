#ifndef QN_PTZ_TOUR_H
#define QN_PTZ_TOUR_H

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMetaType>

#include "ptz_fwd.h"

struct QnPtzTourSpot {
    QnPtzTourSpot(): stayTime(0), speed(1.0) {}
    QnPtzTourSpot(const QString &presetId, qint64 stayTime, qreal speed): presetId(presetId), stayTime(stayTime), speed(speed) {}

    QString presetId;
    qint64 stayTime;
    qreal speed;
};


struct QnPtzTour {
    QnPtzTour() {}
    QnPtzTour(const QString &id, const QString &name, const QnPtzTourSpotList &spots): id(id), name(name), spots(spots) {}

    QString id;
    QString name;
    QnPtzTourSpotList spots;
};

Q_DECLARE_METATYPE(QnPtzTour)
Q_DECLARE_METATYPE(QnPtzTourSpot)

#endif // QN_PTZ_TOUR_H
