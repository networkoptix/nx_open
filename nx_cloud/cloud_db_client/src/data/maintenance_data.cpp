#include "maintenance_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace cdb {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (VmsConnectionData)(VmsConnectionDataList),
    (json),
    _Fields/*,
    (optional, false)*/)

} // namespace api
} // namespace cdb
} // namespace nx
