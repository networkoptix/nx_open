#include "ptz_tour.h"

#include <QtCore/QSet>

#include <core/ptz/ptz_preset.h>

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

        const auto preset = std::find_if(existingPresets.cbegin(), existingPresets.cend(),
            [&](const QnPtzPreset &preset) { return spot.presetId == preset.id; });
        if (preset == existingPresets.cend())
            return false;           // one of the spots does not exist
    }

    if (uniqueSpots.size() < 2)     // all spots are the same
        return false;

    return true;
}

void QnPtzTour::optimize() {
    /* Fix speed & stay time first. */
    for(int i = 0; i < spots.size(); i++) {
        QnPtzTourSpot &spot = spots[i];
        spot.stayTime = qMax(spot.stayTime, 0ll);
        spot.speed = qBound<qreal>(minimalSpeed, spot.speed, maximalSpeed);
    }

    /* Check for duplicates next. */
    for(int i = 0; i < spots.size(); i++) {
        int j = (i + 1) % spots.size();

        QnPtzTourSpot &lastSpot = spots[i];
        QnPtzTourSpot &nextSpot = spots[j];
        if(lastSpot.presetId == nextSpot.presetId) {
            nextSpot.speed = lastSpot.speed;
            nextSpot.stayTime += lastSpot.stayTime;
            spots.removeAt(i);
            i--;
        }
    }
}
