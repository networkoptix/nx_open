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

    friend bool operator==(const QnPtzTourSpot &l, const QnPtzTourSpot &r);

    QString presetId;
    qint64 stayTime;
    qreal speed;
};
#define QnPtzTourSpot_Fields (presetId)(stayTime)(speed)


struct QnPtzTour: public boost::equality_comparable1<QnPtzTour> {
    QnPtzTour() {}
    QnPtzTour(const QnPtzTour &tour): id(tour.id), name(tour.name), spots(tour.spots) {}
    QnPtzTour(const QString &id, const QString &name, const QnPtzTourSpotList &spots): id(id), name(name), spots(spots) {}

    friend bool operator==(const QnPtzTour &l, const QnPtzTour &r);

    /**
     * @brief isValid                   Check validity of the tour.
     * @param existingPresets           List of existing PTZ presets.
     * @return                          True if the tour has at least two different spots and all spots are exist.
     */
    bool isValid(const QnPtzPresetList &existingPresets) const;

    /**
     * @brief optimize                  Fix whatever is fixable in this tour.
     */
    void optimize();


    QString id;
    QString name;
    QnPtzTourSpotList spots;
};
#define QnPtzTour_Fields (id)(name)(spots)

Q_DECLARE_METATYPE(QnPtzTourSpot)
Q_DECLARE_METATYPE(QnPtzTour)
Q_DECLARE_METATYPE(QnPtzTourList)

#endif // QN_PTZ_TOUR_H
