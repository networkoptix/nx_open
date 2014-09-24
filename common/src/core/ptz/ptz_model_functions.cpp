#include <utils/common/model_functions.h>

#include "ptz_tour.h"
#include "ptz_preset.h"
#include "ptz_limits.h"
#include "ptz_data.h"
#include "ptz_object.h"

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qt, Orientations)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnPtzPreset)(QnPtzTourSpot)(QnPtzTour)(QnPtzLimits)(QnPtzObject)(QnPtzData),
    (json)(eq),
    _Fields
)

