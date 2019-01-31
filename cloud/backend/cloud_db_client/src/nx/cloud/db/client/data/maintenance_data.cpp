#include "maintenance_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::cloud::db::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (VmsConnectionData)(VmsConnectionDataList)(Statistics),
    (json),
    _Fields)

} // namespace nx::cloud::db::api
