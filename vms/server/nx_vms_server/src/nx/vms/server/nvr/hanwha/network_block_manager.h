#pragma once

#include <nx/vms/server/nvr/i_network_block_manager.h>

namespace nx::vms::server::nvr::hanwha {

class NetworkBlockManager: public INetworkBlockManager
{
public:
    virtual nx::vms::api::NetworkBlockData state() const override;

    virtual bool setPortPoweringModes(const PoweringModeByPort& poweringModeByPort) override;
};

} // namespace nx::vms::server::nvr::hanwha
