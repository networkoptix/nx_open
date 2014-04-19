#include "item_dewarping_params.h"

#include <utils/common/model_functions.h>
#include <utils/serialization/json_functions.h>
#include <utils/fusion/fusion_adaptor.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnItemDewarpingParams,      (json)(eq),    (enabled)(xAngle)(yAngle)(fov)(panoFactor))
