// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <string_view>

namespace nx::network::rest {

constexpr std::array kApiVersions =
    std::to_array<std::string_view>({"v1", "v2", "v3", "v4", "v5"});
constexpr size_t kLatestApiVersion = kApiVersions.size();

constexpr auto kApiV1 = kApiVersions.cbegin();
constexpr auto kApiV2 = std::next(kApiV1);
constexpr auto kApiV3 = std::next(kApiV2);
constexpr auto kApiV4 = std::next(kApiV3);
constexpr auto kApiV5 = std::next(kApiV4);
constexpr auto kApiEnd = kApiVersions.cend();

} // namespace nx::network::rest
