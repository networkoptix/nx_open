#include <utils/common/model_functions.h>

#include "ptz_tour.h"
#include "ptz_preset.h"
#include "ptz_limits.h"
#include "ptz_data.h"
#include "ptz_object.h"

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qt, Orientations)
QN_DEFINE_LEXICAL_JSON_FUNCTIONS(Qt::Orientations)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPtzPreset,      (json)(eq),    (id)(name))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPtzTourSpot,    (json)(eq),    (presetId)(stayTime)(speed))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPtzTour,        (json)(eq),    (id)(name)(spots))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPtzLimits,      (json)(eq),    (minPan)(maxPan)(minTilt)(maxTilt)(minFov)(maxFov))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPtzObject,      (json)(eq),    (type)(id))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPtzData, (json), (query)(fields)(capabilities)(logicalPosition)(devicePosition)(logicalLimits)(deviceLimits)(flip)(presets)(tours)(activeObject)(homeObject))

