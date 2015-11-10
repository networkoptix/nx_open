#include "api_layout_data.h"
#include "api_model_functions_impl.h"

namespace ec2 {

    ApiLayoutItemData::ApiLayoutItemData() : 
        ApiData()
        , id()
        , flags(0)
        , left(0)
        , top(0)
        , right(0)
        , bottom(0)
        , rotation(0.0)
        , resourceId()
        , resourcePath()
        , zoomLeft(0.0)
        , zoomTop(0.0)
        , zoomRight(0.0)
        , zoomBottom(0.0)
        , zoomTargetId()
        , contrastParams()
        , dewarpingParams()
        , displayInfo(false)
    {}


    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ApiLayoutItemData) (ApiLayoutItemWithRefData) (ApiLayoutData), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields, (optional, true))
} // namespace ec2
