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
    VmsRequestCompletionHandler completionHandler)
{
    RequestContext requestContext;
    requestContext.systemId = targetSystemId;
    requestContext.completionHandler = std::move(completionHandler);

    m_pollable.post(
        [this, requestContext = std::move(requestContext)]() mutable
        {
            m_systemsRequested.insert(requestContext.systemId);

            if (m_paused)
                m_queuedRequests.push(std::move(requestContext));
            else
                reportRequestResult(std::move(requestContext));
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
                auto requestContext = std::move(m_queuedRequests.front());
                m_queuedRequests.pop();
                reportRequestResult(std::move(requestContext));
            }
        });
}

bool VmsGatewayStub::performedRequestToSystem(const std::string& id) const
{
    return m_systemsRequested.find(id) != m_systemsRequested.end();
}

void VmsGatewayStub::failEveryRequestToSystem(const std::string& id)
{
    m_malfunctioningSystems.insert(id);
}

void VmsGatewayStub::reportRequestResult(RequestContext requestContext)
{
    VmsRequestResult vmsRequestResult;

    vmsRequestResult.resultCode = 
        m_malfunctioningSystems.find(requestContext.systemId) == m_malfunctioningSystems.end()
        ? VmsResultCode::ok
        : VmsResultCode::networkError;

    requestContext.completionHandler(std::move(vmsRequestResult));
}

} // namespace test
} // namespace cdb
} // namespace nx
