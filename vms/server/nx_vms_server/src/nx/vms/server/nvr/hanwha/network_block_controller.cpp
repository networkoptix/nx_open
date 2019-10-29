#include "network_block_controller.h"

namespace nx::vms::server::nvr::hanwha {

using namespace nx::vms::api;

nx::vms::api::NetworkBlockData makeFakeData()
{
    NetworkBlockData result;
    result.totalPowerLimitWatts = 100;
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

nx::vms::api::NetworkBlockData NetworkBlockController::state() const
{
    return makeFakeData();
}

QnUuid NetworkBlockController::registerStateChangeHandler(StateChangeHandler stateChangeHandler)
{
    const QnUuid id = QnUuid::createUuid();

    QnMutexLocker lock(&m_handlerMutex);
    m_handlers.emplace(id, std::move(stateChangeHandler));

    return id;
}

void NetworkBlockController::unregisterStateChangeHandler(QnUuid handlerId)
{
    QnMutexLocker lock(&m_handlerMutex);
    m_handlers.erase(handlerId);
}

bool NetworkBlockController::setPortPoweringModes(const PoweringModeByPort& poweringModeByPort)
{
    // TODO: #dmishin implement.
    return false;
}

} // namespace nx::vms::server::nvr::hanwha
