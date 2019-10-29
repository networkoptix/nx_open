#pragma once

#include <nx/vms/server/nvr/i_network_block_controller.h>

#include <nx/utils/thread/mutex.h>

namespace nx::vms::server::nvr::hanwha {

class NetworkBlockController: public INetworkBlockController
{
public:
    virtual nx::vms::api::NetworkBlockData state() const override;

    virtual QnUuid registerStateChangeHandler(StateChangeHandler stateChangeHandler) override;

    virtual void unregisterStateChangeHandler(QnUuid handlerId) override;

    virtual bool setPortPoweringModes(const PoweringModeByPort& poweringModeByPort) override;

private:
    mutable QnMutex m_handlerMutex;
    std::map<QnUuid, StateChangeHandler> m_handlers;
};

} // namespace nx::vms::server::nvr::hanwha
