#include "network_block_state_fetcher.h"

namespace nx::vms::server::nvr::hanwha {

using namespace nx::vms::api;

static const std::chrono::milliseconds kSleepBetweenIterations(1000); //< TODO: change!

static NetworkBlockData makeFakeData()
{
    NetworkBlockData result;
    result.upperPowerLimitWatts = 100;
    result.lowerPowerLimitWatts = 80;
    result.isInPoeOverBudgetMode = false;
    for (int i = 1; i < 9; ++i)
    {
        NetworkPortState state;
        state.portNumber = i;
        state.poweringMode = NetworkPortWithPoweringMode::PoweringMode::automatic;
        state.deviceId = QnUuid::createUuid();
        state.devicePowerConsumptionLimitWatts = i * 3;
        state.devicePowerConsumptionLimitWatts =
            state.devicePowerConsumptionWatts > 6 ? 30 : 7;

        state.linkSpeedMbps = 100;
        state.poweringStatus = NetworkPortState::PoweringStatus::powered;
        result.portStates.push_back(std::move(state));
    }

    return result;
}

NetworkBlockStateFetcher::NetworkBlockStateFetcher(
    std::function<void(const nx::vms::api::NetworkBlockData& state)> stateHandler)
    :
    m_stateHandler(std::move(stateHandler))
{
}

NetworkBlockStateFetcher::~NetworkBlockStateFetcher()
{
    stop();
}

void NetworkBlockStateFetcher::run()
{
    NX_ASSERT(m_stateHandler);

    while (!needToStop())
    {
        std::this_thread::sleep_for(kSleepBetweenIterations);
        m_stateHandler(makeFakeData());
    }
}

} // namespace nx::vms::server::nvr::hanwha
