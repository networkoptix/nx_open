#include "vms_gateway.h"

#include <nx/network/http/rest/http_rest_client.h>

#include "../settings.h"

namespace nx {
namespace cdb {

VmsGateway::VmsGateway(const conf::Settings& settings):
    m_settings(settings)
{
}

VmsGateway::~VmsGateway()
{
    m_asyncCall.pleaseStopSync();

    decltype(m_activeRequests) activeRequests;
    {
        QnMutexLocker lock(&m_mutex);
        m_activeRequests.swap(activeRequests);
    }

    for (auto& val: activeRequests)
        val.second.client->pleaseStopSync();
}

void VmsGateway::merge(
    const std::string& targetSystemId,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    const auto urlBase = m_settings.vmsGateway().url;
    if (urlBase.empty())
    {
        m_asyncCall.post(std::move(completionHandler));
        return;
    }

    const auto requestUrl =
        nx_http::rest::substituteParameters(urlBase, { targetSystemId });

    // TODO: #ak Add request parameters.
    // TODO: #ak Add authentication.

    RequestContext requestContext;
    requestContext.client = std::make_unique<nx_http::AsyncClient>();
    requestContext.completionHandler = std::move(completionHandler);
    auto clientPtr = requestContext.client.get();

    QnMutexLocker lock(&m_mutex);
    m_activeRequests.emplace(clientPtr, std::move(requestContext));
    clientPtr->doGet(
        QUrl(requestUrl.c_str()),
        std::bind(&VmsGateway::reportRequestResult, this, clientPtr));
}

void VmsGateway::reportRequestResult(nx_http::AsyncClient* clientPtr)
{
    nx::utils::MoveOnlyFunc<void()> completionHandler;

    {
        QnMutexLocker lock(&m_mutex);
        const auto it = m_activeRequests.find(clientPtr);
        if (it == m_activeRequests.end())
            return;
        completionHandler = std::move(it->second.completionHandler);
    }

    completionHandler();

    {
        QnMutexLocker lock(&m_mutex);
        const auto it = m_activeRequests.find(clientPtr);
        if (it == m_activeRequests.end())
            return;
        m_activeRequests.erase(it);
    }
}

} // namespace cdb
} // namespace nx
