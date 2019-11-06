#include "network_block_controller.h"

#include <nx/vms/server/nvr/hanwha/network_block_state_fetcher.h>

namespace nx::vms::server::nvr::hanwha {

using namespace nx::vms::api;

NetworkBlockController::NetworkBlockController():
    m_stateFetcher(std::make_unique<NetworkBlockStateFetcher>(
        [this](const nx::vms::api::NetworkBlockData& state) { handleState(state); }))
{
}

NetworkBlockController::~NetworkBlockController()
{
    m_stateFetcher->stop();
}

void NetworkBlockController::start()
{
    m_stateFetcher->start();
}

nx::vms::api::NetworkBlockData NetworkBlockController::state() const
{
    QnMutexLocker lock(&m_mutex);

#if 1
    // TODO: #dmishin fake implementation! Remove it!
    auto adjustedNetworkBlockData = m_lastNetworkBlockState;
    for (auto& portState : adjustedNetworkBlockData.portStates)
    {
        if (auto it = m_portPoweringModes.find(portState.portNumber);
            it != m_portPoweringModes.cend())
        {
            portState.poweringMode = it->second;
        }
    }
#endif

    return adjustedNetworkBlockData;
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
    // TODO: #dmishin this is fake implementation! Don't forget to remove this code.
    QnMutexLocker lock(&m_mutex);
    for (const auto& port: poweringModeByPort)
        m_portPoweringModes[port.portNumber] = port.poweringMode;

    return true;
}

void NetworkBlockController::handleState(const nx::vms::api::NetworkBlockData& networkBlockData)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_lastNetworkBlockState == networkBlockData)
            return;

        m_lastNetworkBlockState = networkBlockData;
    }

    {
        QnMutexLocker lock(&m_handlerMutex);
        for (const auto& [_, handler]: m_handlers)
            handler(networkBlockData);
    }
}

} // namespace nx::vms::server::nvr::hanwha
