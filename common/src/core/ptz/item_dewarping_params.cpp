#include "item_dewarping_params.h"

#include <utils/serialization/model_functions.h>
#include <utils/fusion/fusion_adaptor.h>

QN_DEFINE_STRUCT_FUNCTIONS(QnItemDewarpingParams,      (json)(eq),    (enabled)(xAngle)(yAngle)(fov)(panoFactor))
