#include "item_dewarping_params.h"

#include <QtCore/QtMath>

#include <utils/common/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnItemDewarpingParams,      (json)(eq),    (enabled)(xAngle)(yAngle)(fov)(panoFactor))

QnItemDewarpingParams::QnItemDewarpingParams() : enabled(false), xAngle(0.0), yAngle(0.0), fov(70.0 * M_PI / 180.0), panoFactor(1)
{

}
