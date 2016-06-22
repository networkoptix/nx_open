#include "api_discovery_data.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
            (ApiDiscoveryData) (ApiDiscoverPeerData),
            (ubjson)(xml)(json)(sql_record)(csv_record), _Fields, (optional, true))

    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
            (ApiDiscoveredServerData),
            (ubjson)(xml)(json), _Fields, (optional, true))
} // namespace ec2

void serialize_field(const ec2::ApiDiscoveredServerData &, QVariant *) { return; }
void deserialize_field(const QVariant &, ec2::ApiDiscoveredServerData *) { return; }
