#pragma once

#include <nx/vms/server/nvr/i_network_block_controller.h>

#include <nx/utils/thread/mutex.h>

namespace nx::vms::server::nvr::hanwha {

class NetworkBlockStateFetcher;

class NetworkBlockController: public INetworkBlockController
{
public:
    NetworkBlockController();

    virtual ~NetworkBlockController() override;

    virtual void start() override;

    virtual nx::vms::api::NetworkBlockData state() const override;

    virtual QnUuid registerStateChangeHandler(StateChangeHandler stateChangeHandler) override;

    virtual void unregisterStateChangeHandler(QnUuid handlerId) override;

    virtual bool setPortPoweringModes(const PoweringModeByPort& poweringModeByPort) override;

private:
    void handleState(const nx::vms::api::NetworkBlockData& networkBlockData);

private:
    mutable QnMutex m_mutex;
    mutable QnMutex m_handlerMutex;

    std::map<QnUuid, StateChangeHandler> m_handlers;
    std::unique_ptr<NetworkBlockStateFetcher> m_stateFetcher;
    nx::vms::api::NetworkBlockData m_lastNetworkBlockState;

    // TODO: #dmishin Temporary code! Remove it!
    std::map<int, nx::vms::api::NetworkPortWithPoweringMode::PoweringMode> m_portPoweringModes;
};

} // namespace nx::vms::server::nvr::hanwha
