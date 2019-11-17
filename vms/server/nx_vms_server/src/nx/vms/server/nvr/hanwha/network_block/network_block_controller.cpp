#include "network_block_controller.h"

#include <core/resource/param.h>

#include <nx/vms/server/nvr/hanwha/network_block/network_block_state_fetcher.h>
#include <nx/vms/server/nvr/hanwha/network_block/i_network_block_platform_abstraction.h>

namespace nx::vms::server::nvr::hanwha {

using namespace nx::vms::api;

NetworkBlockController::NetworkBlockController(
    std::unique_ptr<INetworkBlockPlatformAbstraction> platformAbstraction,
    StateHandler stateHandler)
    :
    m_platformAbstraction(std::move(platformAbstraction)),
    m_stateFetcher(std::make_unique<NetworkBlockStateFetcher>(
        m_platformAbstraction.get(),
        [this](const NetworkPortStateList& state) { handleStates(state); })),
    m_handler(std::move(stateHandler))
{
    NX_DEBUG(this, "Creating Hanwha NVR network block controller");
}

NetworkBlockController::~NetworkBlockController()
{
    NX_DEBUG(this, "Destoying Hanwha NVR network block controller");
    m_stateFetcher->stop();
}

void NetworkBlockController::start()
{
    NX_DEBUG(this, "Starting Hanwha NVR network block controller");
    m_stateFetcher->start();
}

NetworkPortStateList NetworkBlockController::state() const
{
    NX_DEBUG(this, "Getting network block state");
    QnMutexLocker lock(&m_mutex);
    return m_lastPortStates;
}

NetworkPortPoeStateList NetworkBlockController::setPoeStates(
    const NetworkPortPoeStateList& poeStates)
{
    NX_DEBUG(this, "Setting PoE states: %1", containerString(poeStates));

    NetworkPortPoeStateList result;
    for (const NetworkPortPoeState& portPoeState: poeStates)
    {
        // TODO: #dmishin something wrong is here. Does setPoeEnabled returns success or actual port
        // PoE state.
        const bool isPoeEnabled = m_platformAbstraction->setPoeEnabled(
            portPoeState.portNumber,
            portPoeState.isPoeEnabled);

        result.push_back({portPoeState.portNumber, isPoeEnabled});
    }

    return result;
}

void NetworkBlockController::handleStates(const NetworkPortStateList& states)
{
    NX_VERBOSE(this, "Handling port states: %1", containerString(states));

    {
        QnMutexLocker lock(&m_mutex);
        m_lastPortStates = states;
    }

    m_handler(states);
}

} // namespace nx::vms::server::nvr::hanwha
