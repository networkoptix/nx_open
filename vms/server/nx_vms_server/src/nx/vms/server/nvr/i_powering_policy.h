#pragma once

#include <nx/vms/api/data/network_block_data.h>
#include <nx/vms/server/nvr/types.h>

namespace nx::vms::server::nvr {

class IPoweringPolicy
{
public:
    enum class PoeState
    {
        undefined,
        normal,
        overBudget,
    };

public:
    virtual ~IPoweringPolicy() = default;

    /**
     * @return A list of port powering modes that has to be changed.
     */
    virtual NetworkPortPoeStateList updatePoeStates(
        const NetworkPortStateList& currentPortStates,
        const std::vector<nx::vms::api::NetworkPortWithPoweringMode>&
            userDefinedPoweringModeList) = 0;

    virtual PoeState poeState() const = 0;

    virtual double upperPowerConsumptionLimitWatts() const = 0;

    virtual double lowerPowerConsumptionLimitWatts() const = 0;

};

} // namespace nx::vms::server::nvr
