#include "discovery_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (DiscoverPeerData)(DiscoveredServerData),
    (ubjson)(json)(xml)(csv_record),
    _Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (DiscoveryData),
    (ubjson)(json)(xml)(csv_record)(sql_record),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
