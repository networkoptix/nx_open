// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::network::maintenance::statistics {

struct NX_NETWORK_API Statistics
{
    std::chrono::milliseconds uptimeMsec{0};
};

#define Statistics_maintenance_Fields (uptimeMsec)

QN_FUSION_DECLARE_FUNCTIONS(Statistics, (json), NX_NETWORK_API)

} // namespace nx::network::maintenance::statistics


