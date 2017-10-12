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
    VmsRequestCompletionHandler completionHandler)
{
    const auto urlBase = m_settings.vmsGateway().url;
    if (urlBase.empty())
    {
        m_asyncCall.post(
            [completionHandler = std::move(completionHandler)]()
            {
                VmsRequestResult vmsRequestResult;
                vmsRequestResult.resultCode = VmsResultCode::invalidData;
                completionHandler(std::move(vmsRequestResult));
            });
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
    VmsRequestCompletionHandler completionHandler;

    {
        QnMutexLocker lock(&m_mutex);
        const auto it = m_activeRequests.find(clientPtr);
        if (it == m_activeRequests.end())
            return;
        completionHandler = std::move(it->second.completionHandler);
    }

    completionHandler(prepareVmsResult(clientPtr));

    {
        QnMutexLocker lock(&m_mutex);
        const auto it = m_activeRequests.find(clientPtr);
        if (it == m_activeRequests.end())
            return;
        m_activeRequests.erase(it);
    }
}

VmsRequestResult VmsGateway::prepareVmsResult(nx_http::AsyncClient* clientPtr)
{
    VmsRequestResult result;
    result.resultCode = VmsResultCode::ok;

    if (!clientPtr->response())
    {
        result.resultCode = VmsResultCode::networkError;
        return result;
    }

    if (!nx_http::StatusCode::isSuccessCode(clientPtr->response()->statusLine.statusCode))
    {
        result.resultCode = VmsResultCode::logicalError;
        return result;
    }

    return result;
}

} // namespace cdb
} // namespace nx
