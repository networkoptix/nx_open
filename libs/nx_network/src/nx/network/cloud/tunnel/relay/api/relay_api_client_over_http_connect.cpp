#include "relay_api_client_over_http_connect.h"

#include <nx/utils/type_utils.h>

namespace nx::cloud::relay::api::detail {

ClientOverHttpConnect::ClientOverHttpConnect(
    const nx::utils::Url& baseUrl,
    ClientFeedbackFunction feedbackFunction)
    :
    base_type(baseUrl, std::move(feedbackFunction))
{
}

void ClientOverHttpConnect::beginListening(
    const std::string& peerName,
    BeginListeningHandler completionHandler)
{
    completionHandler =
        [this, completionHandler = std::move(completionHandler)](
            ResultCode resultCode,
            BeginListeningResponse response,
            std::unique_ptr<network::AbstractStreamSocket> connection)
        {
            giveFeedback(resultCode);
            completionHandler(
                resultCode,
                std::move(response),
                std::move(connection));
        };

    post(
        [this, peerName, completionHandler = std::move(completionHandler)]() mutable
        {
            auto httpClient = std::make_unique<network::http::AsyncClient>();
            httpClient->bindToAioThread(getAioThread());
            auto httpClientPtr = httpClient.get();
            m_activeRequests.push_back(std::move(httpClient));
            httpClientPtr->doConnect(
                url(),
                peerName.c_str(),
                [this, httpClientIter = std::prev(m_activeRequests.end()),
                    completionHandler = std::move(completionHandler)]() mutable
                {
                    processBeginListeningResponse(
                        httpClientIter,
                        std::move(completionHandler));
                });
        });
}

void ClientOverHttpConnect::processBeginListeningResponse(
    std::list<std::unique_ptr<network::aio::BasicPollable>>::iterator httpClientIter,
    BeginListeningHandler completionHandler)
{
    auto httpClient = nx::utils::static_unique_ptr_cast<network::http::AsyncClient>(
        std::move(*httpClientIter));
    m_activeRequests.erase(httpClientIter);

    if (httpClient->failed() || !httpClient->response())
    {
        return completionHandler(
            ResultCode::networkError,
            BeginListeningResponse(),
            nullptr);
    }

    const auto resultCode =
        toResultCode(SystemError::noError, httpClient->response());
    if (httpClient->response()->statusLine.statusCode != network::http::StatusCode::ok)
    {
        return completionHandler(
            resultCode != ResultCode::ok ? resultCode : ResultCode::unknownError,
            BeginListeningResponse(),
            nullptr);
    }

    BeginListeningResponse response;
    deserializeFromHeaders(
        httpClient->response()->headers,
        &response);

    completionHandler(
        api::ResultCode::ok,
        std::move(response),
        httpClient->takeSocket());
}

} // namespace nx::cloud::relay::api::detail
