#pragma once

#include <functional>

#include <nx/vms/api/data/network_block_data.h>

namespace nx::vms::server::nvr {

class INetworkBlockController
{
public:
    using PoweringModeByPort = std::map<int, nx::vms::api::NetworkPortState::PoweringMode>;
    using StateChangeHandler = std::function<void(const nx::vms::api::NetworkBlockData&)>;
public:
    virtual ~INetworkBlockController() = default;

    virtual nx::vms::api::NetworkBlockData state() const = 0;

    virtual QnUuid registerStateChangeHandler(StateChangeHandler stateChangeHandler) = 0;

    virtual void unregisterStateChangeHandler(QnUuid handlerId) = 0;

    virtual bool setPortPoweringModes(const PoweringModeByPort& poweringModeByPort) = 0;
};

} // namespace nx::vms::server::nvr
