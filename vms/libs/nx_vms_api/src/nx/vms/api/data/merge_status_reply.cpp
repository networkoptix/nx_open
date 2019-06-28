#include "merge_status_reply.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (MergeStatusReply),
    (eq)(ubjson)(json)(xml)(sql_record)(csv_record),
    _Fields)

} // namespace nx::vms::api
