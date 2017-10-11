#include "vms_gateway_stub.h"

namespace nx {
namespace cdb {
namespace test {

VmsGatewayStub::~VmsGatewayStub()
{
    m_pollable.pleaseStopSync();
}

void VmsGatewayStub::merge(
    const std::string& targetSystemId,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_pollable.post(
        [this, targetSystemId, completionHandler = std::move(completionHandler)]() mutable
        {
            m_systemsRequested.insert(targetSystemId);

            if (m_paused)
                m_queuedRequests.push(std::move(completionHandler));
            else
                completionHandler();
        });
}

void VmsGatewayStub::pause()
{
    m_paused = true;
}

void VmsGatewayStub::resume()
{
    m_pollable.post(
        [this]()
        {
            m_paused = false;
            while (!m_queuedRequests.empty())
            {
                m_queuedRequests.front()();
                m_queuedRequests.pop();
            }
        });
}

bool VmsGatewayStub::performedRequestToSystem(const std::string& id) const
{
    return m_systemsRequested.find(id) != m_systemsRequested.end();
}

} // namespace test
} // namespace cdb
} // namespace nx
