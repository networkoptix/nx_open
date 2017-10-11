#pragma once

#include <queue>
#include <set>

#include <nx/network/aio/basic_pollable.h>

#include <nx/cloud/cdb/managers/vms_gateway.h>

namespace nx {
namespace cdb {
namespace test {

class VmsGatewayStub:
    public AbstractVmsGateway
{
public:
    virtual ~VmsGatewayStub() override;

    virtual void merge(
        const std::string& targetSystemId,
        nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    void pause();
    void resume();
    
    bool performedRequestToSystem(const std::string& id) const;

private:
    nx::network::aio::BasicPollable m_pollable;
    bool m_paused = false;
    std::queue<nx::utils::MoveOnlyFunc<void()>> m_queuedRequests;
    std::set<std::string> m_systemsRequested;
};

} // namespace test
} // namespace cdb
} // namespace nx
