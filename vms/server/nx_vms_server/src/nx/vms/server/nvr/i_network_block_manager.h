#pragma once

#include <nx/vms/api/data/network_block_data.h>

#include <nx/vms/server/nvr/types.h>

namespace nx::vms::server::nvr {

class INetworkBlockManager
{
public:
    using PoeOverBudgetHandler = std::function<void(const nx::vms::api::NetworkBlockData&)>;
public:
    virtual ~INetworkBlockManager() = default;

    virtual void start() = 0;

    virtual void stop() = 0;

    virtual nx::vms::api::NetworkBlockData state() const = 0;

    virtual bool setPoeModes(
        const std::vector<nx::vms::api::NetworkPortWithPoweringMode>& poweringModes) = 0;

    virtual HandlerId registerPoeOverBudgetHandler(PoeOverBudgetHandler handler) = 0;

    virtual void unregisterPoeOverBudgetHandler(HandlerId handlerId) = 0;
};

} // namespace nx::vms::server::nvr
