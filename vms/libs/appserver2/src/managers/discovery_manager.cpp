// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "discovery_manager.h"

using namespace nx::vms;

namespace ec2 {

nx::vms::api::DiscoveryData toApiDiscoveryData(
    const nx::Uuid &id,
    const nx::utils::Url &url,
    bool ignore)
{
    nx::vms::api::DiscoveryData params;
    params.id = id;
    params.url = url.toString();
    params.ignore = ignore;
    return params;
}

} // namespace ec2
