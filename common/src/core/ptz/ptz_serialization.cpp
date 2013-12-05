#include <utils/common/json.h>

#include "ptz_tour.h"
#include "ptz_preset.h"

QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS(QnPtzPreset,
    ((&QnPtzPreset::id,         &QnPtzPreset::setId,            "id"))
    ((&QnPtzPreset::name,       &QnPtzPreset::setName,          "name")),
)

QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS(QnPtzTourSpot,
    ((&QnPtzTourSpot::presetId, &QnPtzTourSpot::setPresetId,    "presetId"))
    ((&QnPtzTourSpot::stayTime, &QnPtzTourSpot::setStayTime,    "stayTime"))
    ((&QnPtzTourSpot::speed,    &QnPtzTourSpot::setSpeed,       "speed")),
)

QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS(QnPtzTour,
    ((&QnPtzTour::id,           &QnPtzTour::setId,              "id"))
    ((&QnPtzTour::name,         &QnPtzTour::setName,            "name"))
    ((&QnPtzTour::spots,        &QnPtzTour::setSpots,           "spots"))
)
