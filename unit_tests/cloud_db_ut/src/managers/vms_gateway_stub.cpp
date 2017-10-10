#include "vms_gateway_stub.h"

namespace nx {
namespace cdb {
namespace test {

VmsGatewayStub::~VmsGatewayStub()
{
    m_pollable.pleaseStopSync();
}

void VmsGatewayStub::merge(
    const std::string& /*targetSystemId*/,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_pollable.post(std::move(completionHandler));
}

void VmsGatewayStub::pause()
{
    // TODO
}

void VmsGatewayStub::resume()
{
    // TODO
}

} // namespace test
} // namespace cdb
} // namespace nx
