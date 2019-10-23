#include "network_block_manager.h"

namespace nx::vms::server::nvr::hanwha {

nx::vms::api::NetworkBlockData NetworkBlockManager::state() const
{
    return nx::vms::api::NetworkBlockData();
}

bool NetworkBlockManager::setPortPoweringModes(const PoweringModeByPort& poweringModeByPort)
{
    // TODO: #dmishin implement.
    return false;
}

} // namespace nx::vms::server::nvr::hanwha
