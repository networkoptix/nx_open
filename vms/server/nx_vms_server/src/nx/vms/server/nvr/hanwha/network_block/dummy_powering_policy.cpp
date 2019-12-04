#include "dummy_powering_policy.h"

#include <nx/vms/server/nvr/hanwha/utils.h>

namespace nx::vms::server::nvr::hanwha {

DummyPoweringPolicy::DummyPoweringPolicy(
    double lowerConsumptionLimitWatts,
    double upperConsumptionLimitWatts)
    :
    m_lowerConsumptionLimitWatts(lowerConsumptionLimitWatts),
    m_upperConsumptionLimitWatts(upperConsumptionLimitWatts)
{
    NX_ASSERT(m_upperConsumptionLimitWatts >= m_lowerConsumptionLimitWatts);
}

NetworkPortPoeStateList DummyPoweringPolicy::updatePoeStates(
    const NetworkPortStateList& currentPortStates,
    const std::vector<nx::vms::api::NetworkPortWithPoweringMode>&
        /*userDefinedPoweringModeList*/)
{
    const double totalPowerConsumptionWatts = calculateCurrentConsumptionWatts(currentPortStates);
    if (totalPowerConsumptionWatts > m_upperConsumptionLimitWatts)
        m_poeState = PoeState::overBudget;
    else if (totalPowerConsumptionWatts < m_lowerConsumptionLimitWatts)
        m_poeState = PoeState::normal;

    return {};
}

IPoweringPolicy::PoeState DummyPoweringPolicy::poeState() const
{
    return m_poeState;
}

double DummyPoweringPolicy::upperPowerConsumptionLimitWatts() const
{
    return m_upperConsumptionLimitWatts;
}

double DummyPoweringPolicy::lowerPowerConsumptionLimitWatts() const
{
    return m_lowerConsumptionLimitWatts;
}

} // namespace nx::vms::server::nvr::hanwha
