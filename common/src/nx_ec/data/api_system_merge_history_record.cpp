#include "api_system_merge_history_record.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiSystemMergeHistoryRecord), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields)

} // namespace ec2
