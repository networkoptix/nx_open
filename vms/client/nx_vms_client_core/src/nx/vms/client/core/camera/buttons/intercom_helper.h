// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/utils/uuid.h>

namespace nx::vms::client::core {

struct IntercomHelper
{
    static constexpr std::chrono::milliseconds kOpenedDoorDuration = std::chrono::seconds(6);

    static nx::Uuid intercomOpenDoorRuleId(const nx::Uuid& cameraId);
};

} // namespace nx::vms::client::core
