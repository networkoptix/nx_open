#include "p2p_statistics_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (P2pStatisticsData), 
    (ubjson)(json), 
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
