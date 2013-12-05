#include <utils/common/json.h>

#include "ptz_tour.h"
#include "ptz_preset.h"

QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnPtzPreset,      (id)(name))
QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnPtzTourSpot,    (presetId)(stayTime)(speed))
QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnPtzTour,        (id)(name)(spots))
