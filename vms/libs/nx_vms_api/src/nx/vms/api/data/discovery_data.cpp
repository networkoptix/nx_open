// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "discovery_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    DiscoverPeerData, (ubjson)(json)(xml)(csv_record), DiscoverPeerData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    DiscoveredServerData, (ubjson)(json)(xml)(csv_record), DiscoveredServerData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    DiscoveryData, (ubjson)(json)(xml)(csv_record)(sql_record), DiscoveryData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
