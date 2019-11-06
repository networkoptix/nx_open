#pragma once

#include <functional>

#include <nx/vms/server/nvr/i_startable.h>
#include <nx/vms/api/data/network_block_data.h>

namespace nx::vms::server::nvr {

class INetworkBlockController: public IStartable
{
public:
    using PoweringModeByPort = std::vector<nx::vms::api::NetworkPortWithPoweringMode>;
    using StateChangeHandler = std::function<void(const nx::vms::api::NetworkBlockData&)>;
public:
    virtual ~INetworkBlockController() = default;

    virtual void start() override = 0;

    virtual nx::vms::api::NetworkBlockData state() const = 0;

    virtual QnUuid registerStateChangeHandler(StateChangeHandler stateChangeHandler) = 0;

    virtual void unregisterStateChangeHandler(QnUuid handlerId) = 0;

    virtual bool setPortPoweringModes(const PoweringModeByPort& poweringModeByPort) = 0;
};

} // namespace nx::vms::server::nvr
