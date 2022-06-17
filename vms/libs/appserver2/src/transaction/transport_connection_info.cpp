// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "transport_connection_info.h"

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/json_functions.h>

namespace ec2 {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnTransportConnectionInfo, (json), QnTransportConnectionInfo_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ConnectionInfos, (json), ConnectionInfos_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(P2pStats, (json), P2pStats_Fields)

} // namespace ec2
