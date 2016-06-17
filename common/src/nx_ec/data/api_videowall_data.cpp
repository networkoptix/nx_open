#include "api_videowall_data.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ApiVideowallItemData) (ApiVideowallScreenData) (ApiVideowallMatrixItemData) (ApiVideowallMatrixData) (ApiVideowallData) (ApiVideowallControlMessageData) (ApiVideowallItemWithRefData) (ApiVideowallScreenWithRefData) (ApiVideowallMatrixItemWithRefData) (ApiVideowallMatrixWithRefData), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields, (optional, true))
} // namespace ec2
