#ifndef QN_PTZ_TOUR_H
#define QN_PTZ_TOUR_H

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMetaType>

#include <utils/common/json.h>

#include "ptz_fwd.h"

class QnPtzTourSpot {
public:
    QnPtzTourSpot(): m_stayTime(0), m_speed(1.0) {}
    QnPtzTourSpot(const QString &presetId, qint64 stayTime, qreal speed): m_presetId(presetId), m_stayTime(stayTime), m_speed(speed) {}

    bool isNull() const { return m_presetId.isNull(); }

    const QString &presetId() const { return m_presetId; }
    void setPresetId(const QString &presetId) { m_presetId = presetId; }

    qint64 stayTime() const { return m_stayTime; }
    void setStayTime(qint64 stayTime) { m_stayTime = stayTime; }

    qreal speed() const { return m_speed; }
    void setSpeed(qreal speed) { m_speed = speed; }

private:
    QString m_presetId;
    qint64 m_stayTime;
    qreal m_speed;
};


class QnPtzTour {
public:
    QnPtzTour() {}
    QnPtzTour(const QString &id, const QString &name, const QnPtzTourSpotList &spots): m_id(id), m_name(name), m_spots(spots) {}

    bool isNull() const { return m_id.isNull(); }

    const QString &id() const { return m_id; }
    void setId(const QString &id) { m_id = id; }

    const QString &name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    const QnPtzTourSpotList &spots() const { return m_spots; }
    void setSpots(const QnPtzTourSpotList &spots) { m_spots = spots; }

private:
    QString m_id;
    QString m_name;
    QnPtzTourSpotList m_spots;
};

Q_DECLARE_METATYPE(QnPtzTour)
Q_DECLARE_METATYPE(QnPtzTourSpot)

#endif // QN_PTZ_TOUR_H
