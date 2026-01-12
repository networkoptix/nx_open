// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

namespace nx::vms::client::core {
namespace timeline {

template<typename DataList>
struct DataListTraits
{
    static std::chrono::milliseconds timestamp(const auto& item);
};

} // namespace timeline
} // namespace nx::vms::client::core
