#include "api_layout_data.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {

ApiLayoutItemData::ApiLayoutItemData():
    flags(0),
    left(0),
    top(0),
    right(0),
    bottom(0),
    rotation(0.0),
    resourceId(),
    resourcePath(),
    zoomLeft(0.0),
    zoomTop(0.0),
    zoomRight(0.0),
    zoomBottom(0.0),
    zoomTargetId(),
    contrastParams(),
    dewarpingParams(),
    displayInfo(false)
{
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiLayoutItemData)(ApiLayoutData),
    (ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

} // namespace ec2
