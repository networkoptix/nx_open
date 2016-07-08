#include "item_dewarping_params.h"

#include <QtCore/QtMath>

#include <nx/fusion/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((QnItemDewarpingParams), (json)(eq), _Fields)

QnItemDewarpingParams::QnItemDewarpingParams() : enabled(false), xAngle(0.0), yAngle(0.0), fov(70.0 * M_PI / 180.0), panoFactor(1)
{

}
