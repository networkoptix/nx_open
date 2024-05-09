// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <string_view>
#include <string>

#include "data_macros.h"

constexpr std::array<std::string_view, 3> kRestApiVersions{"v1", "v2", "v3"};
constexpr auto kRestApiV1 = kRestApiVersions.begin();
constexpr auto kRestApiV2 = std::next(kRestApiV1);
constexpr auto kRestApiV3 = std::next(kRestApiV2);
constexpr std::string_view kLegacyApi = "legacy";

namespace nx::vms::api {

struct NX_VMS_API RestApiVersions
{
    std::string min;
    std::string max;

    static RestApiVersions current();
};
#define RestApiVersions_Fields (min)(max)
NX_VMS_API_DECLARE_STRUCT_EX(RestApiVersions, (json));

} // namespace nx::vms::api
