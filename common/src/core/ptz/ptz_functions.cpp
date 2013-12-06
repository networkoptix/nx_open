#include <utils/common/json.h>
#include <utils/common/struct_functions.h>

#include "ptz_tour.h"
#include "ptz_preset.h"

QN_DEFINE_STRUCT_FUNCTIONS(QnPtzPreset,      (json)(eq),    (id)(name))
QN_DEFINE_STRUCT_FUNCTIONS(QnPtzTourSpot,    (json)(eq),    (presetId)(stayTime)(speed))
QN_DEFINE_STRUCT_FUNCTIONS(QnPtzTour,        (json)(eq),    (id)(name)(spots))

