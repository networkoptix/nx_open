// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_feature.h"

namespace nx::vms::client::core {

CloudFeatures allCloudFeatures()
{
    CloudFeatures result;
    for (const auto feature: nx::reflect::enumeration::allEnumValues<CloudFeature>())
        result.setFlag(feature);
    return result;
}

} // namespace nx::vms::client::core
