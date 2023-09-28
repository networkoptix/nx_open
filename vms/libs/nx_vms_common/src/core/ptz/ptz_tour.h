// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QString>

#include <nx/reflect/instrument.h>

#include "ptz_fwd.h"

struct NX_VMS_COMMON_API QnPtzTourSpot
{
    QnPtzTourSpot(): stayTime(0), speed(1.0) {}
    QnPtzTourSpot(
        const QString &presetId,
        qint64 stayTime,
        qreal speed)
        :
        presetId(presetId),
        stayTime(stayTime),
        speed(speed)
    {
    }

    bool operator==(const QnPtzTourSpot& other) const;

    QString presetId;
    qint64 stayTime;
    qreal speed;
};
#define QnPtzTourSpot_Fields (presetId)(stayTime)(speed)
NX_REFLECTION_INSTRUMENT(QnPtzTourSpot, QnPtzTourSpot_Fields)

struct NX_VMS_COMMON_API QnPtzTour
{
    QnPtzTour() {}

    QnPtzTour(const QString &id, const QString &name, const QnPtzTourSpotList &spots):
        id(id),
        name(name),
        spots(spots)
    {
    }

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

    bool operator==(const QnPtzTour& other) const = default;

    QString id;
    QString name;
    QnPtzTourSpotList spots;
};
#define QnPtzTour_Fields (id)(name)(spots)
