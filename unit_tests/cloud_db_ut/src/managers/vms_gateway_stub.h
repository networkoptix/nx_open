#pragma once

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

private:
    nx::network::aio::BasicPollable m_pollable;
};

} // namespace test
} // namespace cdb
} // namespace nx
