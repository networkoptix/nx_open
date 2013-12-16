#include "ptz_tour.h"

#include <QtCore/QSet>

#include <core/ptz/ptz_preset.h>

#include <utils/common/container.h>

namespace {
    const qreal minimalSpeed = 0.01;
    const qreal maximalSpeed = 1.0;
}

bool QnPtzTour::isValid(const QnPtzPresetList &existingPresets) const {
    if(id.isEmpty() || spots.isEmpty())
        return false;

    QSet<QString> uniqueSpots;

    for(int i = 0; i < spots.size(); i++) {
        QnPtzTourSpot spot = spots[i];
        uniqueSpots << spot.presetId;

        int index = qnIndexOf(existingPresets, [&](const QnPtzPreset &preset) { return spot.presetId == preset.id; });
        if(index < 0)
            return false;           // one of the spots does not exist
    }

    if (uniqueSpots.size() < 2)     // all spots are the same
        return false;

    return true;
}

void QnPtzTour::validateSpots() {
    for(int i = 0; i < spots.size(); i++) {
        QnPtzTourSpot &spot = spots[i];
        spot.stayTime = qMax(spot.stayTime, 0ll);
        spot.speed = qBound<qreal>(minimalSpeed, spot.speed, maximalSpeed);
    }
}
