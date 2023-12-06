// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "network_block_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/reflect/compare.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(NetworkPortWithPoweringMode, (json),
    NetworkPortWithPoweringMode_Fields);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(NetworkPortState, (json), NetworkPortState_Fields);
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(NetworkBlockData, (json), NetworkBlockData_Fields);
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(NvrNetworkRequest, (json), NvrNetworkRequest_Fields);

bool NetworkPortState::operator==(const NetworkPortState& other) const
{
    return nx::reflect::equals(*this, other);
}

bool NetworkBlockData::operator==(const NetworkBlockData& other) const
{
    return nx::reflect::equals(*this, other);
}

} // namespace nx::vms::api
