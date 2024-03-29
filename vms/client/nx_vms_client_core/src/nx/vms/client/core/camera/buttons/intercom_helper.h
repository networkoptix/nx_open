// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/utils/uuid.h>

namespace nx::vms::client::core {

struct IntercomHelper
{
    /**
     * In 5.0 open door software trigger has 6 seconds duration and it should be managed by the
     * client.
     * In 5.1 duration is managed (replaced) by server, and it is safe to send any value -
     * it will be ignored.
     * For compatibility with old servers the client should continue to send the deprecated duration
     * value.
     */
    static constexpr std::chrono::milliseconds kOpenedDoorDuration = std::chrono::seconds(6);

    static nx::Uuid intercomOpenDoorRuleId(const nx::Uuid& cameraId);
};

} // namespace nx::vms::client::core
