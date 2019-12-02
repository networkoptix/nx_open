#pragma once

#include <nx/vms/server/nvr/i_powering_policy.h>

namespace nx::vms::server::nvr::hanwha {

/**
 * This policy doesn't require any change of powering modes since it fully relies on the driver
 *     which is supposed to manage PoE.
 */
class DummyPoweringPolicy: public IPoweringPolicy
{
public:
    DummyPoweringPolicy(double lowerConsumptionLimitWatts, double upperConsumptionLimitWatts);

    virtual NetworkPortPoeStateList updatePoeStates(
        const NetworkPortStateList& currentPortStates,
        const std::vector<nx::vms::api::NetworkPortWithPoweringMode>&
            userDefinedPoweringModeList) override;

    virtual PoeState poeState() const override;

    virtual double upperPowerConsumptionLimitWatts() const override;

    virtual double lowerPowerConsumptionLimitWatts() const override;

private:
    PoeState m_poeState = PoeState::normal;
    double m_lowerConsumptionLimitWatts = 0.0;
    double m_upperConsumptionLimitWatts = 0.0;
};

} // namespace nx::vms::server::nvr::hanwha
