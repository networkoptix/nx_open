#include "api_layout_data.h"
#include "api_model_functions_impl.h"

namespace ec2 {
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ApiLayoutItemData) (ApiLayoutItemWithRefData) (ApiLayoutData), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields)
} // namespace ec2
