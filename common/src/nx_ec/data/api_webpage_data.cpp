#include "api_webpage_data.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiWebPageData), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields)

} // namespace ec2
