#include <utils/common/model_functions.h>

#include "ptz_tour.h"
#include "ptz_preset.h"
#include "ptz_limits.h"
#include "ptz_data.h"

QN_DEFINE_STRUCT_FUNCTIONS(QnPtzPreset,      (json)(eq),    (id)(name))
QN_DEFINE_STRUCT_FUNCTIONS(QnPtzTourSpot,    (json)(eq),    (presetId)(stayTime)(speed))
QN_DEFINE_STRUCT_FUNCTIONS(QnPtzTour,        (json)(eq),    (id)(name)(spots))
QN_DEFINE_STRUCT_FUNCTIONS(QnPtzLimits,      (json)(eq),    (minPan)(maxPan)(minTilt)(maxTilt)(minFov)(maxFov))

QN_DEFINE_ENUM_CAST_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qt::Orientations, static)

QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnPtzData, (fields)(capabilities)(logicalPosition)(devicePosition)(logicalLimits)(deviceLimits)(flip)(presets)(tours))

