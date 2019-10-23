#pragma once

#include <nx/vms/api/data/network_block_data.h>

namespace nx::vms::server::nvr {

class INetworkBlockManager
{
public:
    using PoweringModeByPort = std::map<int, nx::vms::api::NetworkPortState::PoweringMode>;
public:
    virtual ~INetworkBlockManager() = default;

    virtual nx::vms::api::NetworkBlockData state() const = 0;

    virtual bool setPortPoweringModes(const PoweringModeByPort& poweringModeByPort) = 0;
};

} // namespace nx::vms::server::nvr
