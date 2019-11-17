#include "network_block_manager.h"

#include <chrono>

#include <utils/common/synctime.h>

#include <core/resource/param.h>
#include <core/resource/media_server_resource.h>

#include <nx/fusion/model_functions.h>

#include <nx/vms/server/event/event_connector.h>
#include <nx/vms/event/events/poe_over_budget_event.h>

#include <nx/vms/server/nvr/hanwha/utils.h>
#include <nx/vms/server/nvr/hanwha/network_block/network_block_controller.h>
#include <nx/vms/server/nvr/hanwha/network_block/i_network_block_platform_abstraction.h>

namespace nx::vms::server::nvr::hanwha {

using namespace nx::vms::event;

NetworkBlockManager::NetworkBlockManager(
    QnMediaServerResourcePtr currentServer,
    std::unique_ptr<INetworkBlockPlatformAbstraction> platformAbstraction,
    std::unique_ptr<IPoweringPolicy> poweringPolicy)
    :
    m_currentServer(currentServer),
    m_networkBlockController(
        std::make_unique<NetworkBlockController>(
            std::move(platformAbstraction),
            [this](const NetworkPortStateList& state) { handleState(state); })),
    m_poweringPolicy(std::move(poweringPolicy))
{
    NX_DEBUG(this, "Creating the network block manager");
}

NetworkBlockManager::~NetworkBlockManager()
{
    NX_DEBUG(this, "Destroying the network block manager");
}

void NetworkBlockManager::start()
{
    m_networkBlockController->start();
    m_networkBlockController->setPoeStates(
        fromApiPoweringModes(getUserDefinedPortPoweringModes()));
}

nx::vms::api::NetworkBlockData NetworkBlockManager::state() const
{
    NX_DEBUG(this, "Getting network block state");
    return toApiNetworkBlockData(m_networkBlockController->state());
}

bool NetworkBlockManager::setPoeModes(
    const std::vector<nx::vms::api::NetworkPortWithPoweringMode>& portPoweringModes)
{
    QnMutexLocker lock(&m_mutex);
    saveUserDefinedPortPoweringModes(portPoweringModes);
    m_networkBlockController->setPoeStates(fromApiPoweringModes(portPoweringModes));

    return true; //< TODO: #dmishin handle errors.
}

QnUuid NetworkBlockManager::registerPoeOverBudgetHandler(PoeOverBudgetHandler handler)
{
    QnMutexLocker lock(&m_handlerMutex);
    const QnUuid handlerId = getId(m_poeOverBudgetHandlers);
    NX_DEBUG(this, "Registering PoE over budget handler, id: %1", handlerId);
    m_poeOverBudgetHandlers.emplace(handlerId, std::move(handler));
    return handlerId;
}

void NetworkBlockManager::unregisterPoeOverBudgetHandler(QnUuid handlerId)
{
    NX_DEBUG(this, "Unregistering PoE over budget handler, id %1", handlerId);

    QnMutexLocker lock(&m_handlerMutex);
    if (const auto it = m_poeOverBudgetHandlers.find(handlerId);
        it != m_poeOverBudgetHandlers.cend())
    {
        m_poeOverBudgetHandlers.erase(handlerId);
    }
}

void NetworkBlockManager::handleState(const NetworkPortStateList& portStates)
{
    NetworkPortPoeStateList portPoeStatesToChange;
    {
        QnMutexLocker lock(&m_mutex);
        portPoeStatesToChange =
            m_poweringPolicy->updatePoeStates(
                portStates,
                getUserDefinedPortPoweringModes());
    }

    notifyPoeOverBudgetChanged(portStates);

    if (!portPoeStatesToChange.empty())
        m_networkBlockController->setPoeStates(portPoeStatesToChange);
}

std::vector<nx::vms::api::NetworkPortState> NetworkBlockManager::toApiPortStates(
    const NetworkPortStateList& states) const
{
    std::vector<nx::vms::api::NetworkPortState> result;
    for (const auto& portState: states)
    {
        nx::vms::api::NetworkPortState apiPortState;
        apiPortState.portNumber = portState.portNumber;
        apiPortState.poweringMode = getPoweringMode(portState);
        apiPortState.macAddress = portState.macAddress.toString();
        apiPortState.devicePowerConsumptionWatts = portState.devicePowerConsumptionWatts;
        apiPortState.devicePowerConsumptionLimitWatts = portState.devicePowerConsumptionLimitWatts;
        apiPortState.linkSpeedMbps = portState.linkSpeedMbps;
        apiPortState.poweringStatus = getPoweringStatus(portState);

        NX_VERBOSE(this, "Port: %1, Consumption %2, Limit: %3",
            apiPortState.portNumber,
            apiPortState.devicePowerConsumptionWatts,
            apiPortState.devicePowerConsumptionLimitWatts);

        result.push_back(std::move(apiPortState));
    }

    return result;
}

nx::vms::api::NetworkBlockData NetworkBlockManager::toApiNetworkBlockData(
        const NetworkPortStateList& states) const
{
    nx::vms::api::NetworkBlockData networkBlockData;
    networkBlockData.lowerPowerLimitWatts = m_poweringPolicy->lowerPowerConsumptionLimitWatts();
    networkBlockData.upperPowerLimitWatts = m_poweringPolicy->upperPowerConsumptionLimitWatts();
    networkBlockData.isInPoeOverBudgetMode =
        m_poweringPolicy->poeState() == IPoweringPolicy::PoeState::overBudget;

    networkBlockData.portStates = toApiPortStates(states);

    return networkBlockData;
}

NetworkPortPoeStateList NetworkBlockManager::fromApiPoweringModes(
    const std::vector<nx::vms::api::NetworkPortWithPoweringMode>& apiPoweringModes) const
{
    using PoweringMode = nx::vms::api::NetworkPortWithPoweringMode::PoweringMode;

    NetworkPortPoeStateList result;
    for (const auto& portPoweringMode: apiPoweringModes)
    {
        result.push_back({
            portPoweringMode.portNumber,
            portPoweringMode.poweringMode != PoweringMode::off});
    }

    return result;
}

nx::vms::api::NetworkPortState::PoweringStatus NetworkBlockManager::getPoweringStatus(
    const NetworkPortState& portState) const
{
    if (portState.linkSpeedMbps == 0)
        return nx::vms::api::NetworkPortState::PoweringStatus::disconnected;

    if (!qFuzzyIsNull(portState.devicePowerConsumptionWatts))
        return nx::vms::api::NetworkPortState::PoweringStatus::powered;

    return nx::vms::api::NetworkPortState::PoweringStatus::connected;
}

nx::vms::api::NetworkPortWithPoweringMode::PoweringMode NetworkBlockManager::getPoweringMode(
    const NetworkPortState& portState) const
{
    const auto userDefinedPoweringModes = getUserDefinedPortPoweringModes();
    for (const auto& portWithPoweringMode: userDefinedPoweringModes)
    {
        if (portWithPoweringMode.portNumber == portState.portNumber)
            return portWithPoweringMode.poweringMode;
    }

    return nx::vms::api::NetworkPortWithPoweringMode::PoweringMode::automatic;
}

std::vector<nx::vms::api::NetworkPortWithPoweringMode>
    NetworkBlockManager::getUserDefinedPortPoweringModes() const
{
    // TODO: #dmishin default port states.
    bool success = false;
    const auto portPoweringModes = QJson::deserialized(
        m_currentServer->getProperty(
            ResourcePropertyKey::Server::kNvrPoePortPoweringModes).toUtf8(),
        std::vector<nx::vms::api::NetworkPortWithPoweringMode>(),
        &success);

    if (!success)
        return {};

    return portPoweringModes;
}

void NetworkBlockManager::saveUserDefinedPortPoweringModes(
    const std::vector<nx::vms::api::NetworkPortWithPoweringMode>& portPoweringModes)
{
    using PoweringMode = nx::vms::api::NetworkPortWithPoweringMode::PoweringMode;

    std::map<int, PoweringMode> poweringModeByPort;
    const auto currentUserDefinedPoweringModes = getUserDefinedPortPoweringModes();
    for (const auto& portWithPoweringMode: currentUserDefinedPoweringModes)
        poweringModeByPort[portWithPoweringMode.portNumber] = portWithPoweringMode.poweringMode;

    for (const auto& portWithPoweringMode: portPoweringModes)
        poweringModeByPort[portWithPoweringMode.portNumber] = portWithPoweringMode.poweringMode;

    std::vector<nx::vms::api::NetworkPortWithPoweringMode> newPoweringModes;
    for (const auto& [portNumber, poweringMode]: poweringModeByPort)
    {
        nx::vms::api::NetworkPortWithPoweringMode portWithPoweringMode;
        portWithPoweringMode.portNumber = portNumber;
        portWithPoweringMode.poweringMode = poweringMode;
        newPoweringModes.push_back(portWithPoweringMode);
    }

    m_currentServer->setProperty(
        ResourcePropertyKey::Server::kNvrPoePortPoweringModes,
        QString::fromUtf8(QJson::serialized(newPoweringModes)));

    m_currentServer->savePropertiesAsync();
}

void NetworkBlockManager::notifyPoeOverBudgetChanged(const NetworkPortStateList& portStates)
{
    nx::vms::api::NetworkBlockData networkBlockData;
    {
        QnMutexLocker lock(&m_mutex);
        const IPoweringPolicy::PoeState newPoeState = m_poweringPolicy->poeState();
        if (newPoeState == m_lastPoeState)
            return;


        NX_DEBUG(this,
            "PoE state changed: new PoE state is %1",
            (newPoeState == IPoweringPolicy::PoeState::overBudget) ? "over budget" : "normal");

        m_lastPoeState = newPoeState;
        networkBlockData = toApiNetworkBlockData(portStates);
    }

    QnMutexLocker lock(&m_handlerMutex);
    NX_DEBUG(this, "Calling PoE over budget handlers, handlers count: %1",
        m_poeOverBudgetHandlers.size());

    for (const auto& [_, handler]: m_poeOverBudgetHandlers)
        handler(networkBlockData);
}

} // namespace nx::vms::server::nvr::hanwha
