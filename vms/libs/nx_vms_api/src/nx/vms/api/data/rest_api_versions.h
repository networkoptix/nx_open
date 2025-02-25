// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <string_view>

constexpr std::array<std::string_view, 4> kRestApiVersions{"v1", "v2", "v3", "v4"};

constexpr auto kRestApiV1 = kRestApiVersions.cbegin();
constexpr auto kRestApiV2 = std::next(kRestApiV1);
constexpr auto kRestApiV3 = std::next(kRestApiV2);
constexpr auto kRestApiV4 = std::next(kRestApiV3);
constexpr auto kRestApiEnd = kRestApiVersions.cend();

constexpr std::string_view kLegacyApi = "legacy";
