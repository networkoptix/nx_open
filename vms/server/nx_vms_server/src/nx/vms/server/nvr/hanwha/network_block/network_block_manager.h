#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/utils/thread/mutex.h>
#include <nx/vms/server/nvr/i_powering_policy.h>
#include <nx/vms/server/nvr/i_network_block_manager.h>

namespace nx::vms::server::nvr::hanwha {

class NetworkBlockController;
class INetworkBlockPlatformAbstraction;

class NetworkBlockManager: public INetworkBlockManager
{
public:
    NetworkBlockManager(
        QnMediaServerResourcePtr currentServer,
        std::unique_ptr<INetworkBlockPlatformAbstraction> platformAbstraction,
        std::unique_ptr<IPoweringPolicy> poweringPolicy);

    virtual ~NetworkBlockManager() override;

    virtual void start() override;

    virtual void stop() override;

    virtual nx::vms::api::NetworkBlockData state() const override;

    virtual bool setPoeModes(
        const std::vector<nx::vms::api::NetworkPortWithPoweringMode>& poeModes) override;

    virtual HandlerId registerPoeOverBudgetHandler(PoeOverBudgetHandler handler) override;

    virtual void unregisterPoeOverBudgetHandler(HandlerId handlerId) override;

private:
    void handleState(const NetworkPortStateList& state);

    std::vector<nx::vms::api::NetworkPortWithPoweringMode> getUserDefinedPortPoweringModes() const;
    void saveUserDefinedPortPoweringModes(
        const std::vector<nx::vms::api::NetworkPortWithPoweringMode>& portPoweringModes);

    void notifyPoeOverBudgetChanged(const NetworkPortStateList& portStates);

    std::vector<nx::vms::api::NetworkPortState> toApiPortStates(
        const NetworkPortStateList& states) const;

    nx::vms::api::NetworkBlockData toApiNetworkBlockData(const NetworkPortStateList& states) const;

    NetworkPortPoeStateList fromApiPoweringModes(
        const std::vector<nx::vms::api::NetworkPortWithPoweringMode>& poweringModes) const;

    nx::vms::api::NetworkPortState::PoweringStatus getPoweringStatus(
        const NetworkPortState& portState) const;

    nx::vms::api::NetworkPortWithPoweringMode::PoweringMode getPoweringMode(
        const NetworkPortState& portState) const;

private:
    mutable QnMutex m_mutex;
    mutable QnMutex m_handlerMutex;

    QnMediaServerResourcePtr m_currentServer;

    QnUuid m_networkBlockControllerHandlerId;
    std::map<HandlerId, PoeOverBudgetHandler> m_poeOverBudgetHandlers;
    IPoweringPolicy::PoeState m_lastPoeState = IPoweringPolicy::PoeState::normal;

    std::unique_ptr<NetworkBlockController> m_networkBlockController;
    std::unique_ptr<IPoweringPolicy> m_poweringPolicy;

    HandlerId m_maxHandlerId = 0;

    bool m_isStopped = false;
};

} // namespace nx::vms::server::nvr::hanwha
