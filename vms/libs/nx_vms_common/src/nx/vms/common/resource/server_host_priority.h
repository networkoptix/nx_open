// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::network { class HostAddress; }

namespace nx::vms::common {

/**
 * Order of the possible server hosts by their importance for the user.
 */
enum class ServerHostPriority
{
    dns,
    localIpV4,
    remoteIp,
    localIpV6,
    localHost,
    cloud,
};

NX_VMS_COMMON_API ServerHostPriority serverHostPriority(const nx::network::HostAddress& host);

} // namespace nx::vms::common
