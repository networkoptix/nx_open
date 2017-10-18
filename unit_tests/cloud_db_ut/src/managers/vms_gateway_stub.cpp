#include "vms_gateway_stub.h"

namespace nx {
namespace cdb {
namespace test {

VmsGatewayStub::~VmsGatewayStub()
{
    m_pollable.pleaseStopSync();
}

void VmsGatewayStub::merge(
    const std::string& username,
    const std::string& targetSystemId,
    const std::string& /*idOfSystemToMergeTo*/,
    VmsRequestCompletionHandler completionHandler)
{
    RequestContext requestContext;
    requestContext.params.username = username;
    requestContext.params.targetSystemId = targetSystemId;
    requestContext.completionHandler = std::move(completionHandler);

    m_pollable.post(
        [this, requestContext = std::move(requestContext)]() mutable
        {
            m_systemsRequested.insert(requestContext.params.targetSystemId);

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

MergeRequestParameters VmsGatewayStub::popRequest()
{
    return m_performedRequestsQueue.pop();
}

void VmsGatewayStub::failEveryRequestToSystem(const std::string& id)
{
    m_malfunctioningSystems.insert(id);
}

void VmsGatewayStub::reportRequestResult(RequestContext requestContext)
{
    VmsRequestResult vmsRequestResult;

    vmsRequestResult.resultCode = 
        m_malfunctioningSystems.find(requestContext.params.targetSystemId) 
            == m_malfunctioningSystems.end()
        ? VmsResultCode::ok
        : VmsResultCode::networkError;

    m_performedRequestsQueue.push(requestContext.params);

    requestContext.completionHandler(std::move(vmsRequestResult));
}

} // namespace test
} // namespace cdb
} // namespace nx
