#include "statistics_data.h"

#include <nx/fusion/model_functions.h>

using namespace nx::vms;

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    STATISTICS_DATA_TYPES, (ubjson)(xml)(json)(csv_record), _Fields)

} // namespace nx::vms::api
