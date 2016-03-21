#include "api_discovery_data.h"
#include "api_model_functions_impl.h"

namespace ec2 {
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
            (ApiDiscoveryData) (ApiDiscoverPeerData),
            (ubjson)(xml)(json)(sql_record)(csv_record), _Fields, (optional, true))

    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
            (ApiDiscoveredServerData),
            (ubjson)(xml)(json), _Fields, (optional, true))
} // namespace ec2
