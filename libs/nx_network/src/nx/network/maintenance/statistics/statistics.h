// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>

#include <nx/reflect/instrument.h>

namespace nx::network::maintenance::statistics {

struct NX_NETWORK_API Statistics
{
    std::chrono::milliseconds uptimeMsec{0};
};

NX_REFLECTION_INSTRUMENT(Statistics, (uptimeMsec))

} // namespace nx::network::maintenance::statistics
