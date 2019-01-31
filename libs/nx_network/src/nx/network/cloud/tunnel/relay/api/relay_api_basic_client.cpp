#include "relay_api_basic_client.h"

#include <nx/network/http/custom_headers.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/type_utils.h>

#include "relay_api_http_paths.h"

namespace nx::cloud::relay::api {

BasicClient::BasicClient(
    const nx::utils::Url& baseUrl,
    ClientFeedbackFunction feedbackFunction)
    :
    m_baseUrl(baseUrl),
    m_feedbackFunction(std::move(feedbackFunction)),
    m_prevSysErrorCode(SystemError::noError)
{
}

void BasicClient::bindToAioThread(
    network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    for (const auto& request : m_activeRequests)
        request->bindToAioThread(aioThread);
}

void BasicClient::startSession(
    const std::string& desiredSessionId,
    const std::string& targetPeerName,
    StartClientConnectSessionHandler completionHandler)
{
    using namespace std::placeholders;

    CreateClientSessionRequest request;
    request.desiredSessionId = desiredSessionId;
    request.targetPeerName = targetPeerName;

    const auto requestPath = nx::network::http::rest::substituteParameters(
        kServerClientSessionsPath, { targetPeerName });

    issueRequest<CreateClientSessionRequest, CreateClientSessionResponse>(
        std::move(request),
        kServerClientSessionsPath,
        { targetPeerName },
        [this, completionHandler = std::move(completionHandler), requestPath](
            const std::string contentLocationUrl,
            ResultCode resultCode,
            CreateClientSessionResponse response)
        {
            giveFeedback(resultCode);

            response.actualRelayUrl =
                prepareActualRelayUrl(contentLocationUrl, requestPath);

            completionHandler(resultCode, std::move(response));
        });
}

nx::utils::Url BasicClient::url() const
{
    return m_baseUrl;
}

SystemError::ErrorCode BasicClient::prevRequestSysErrorCode() const
{
    return m_prevSysErrorCode;
}

void BasicClient::stopWhileInAioThread()
{
    m_activeRequests.clear();
}

ResultCode BasicClient::toUpgradeResultCode(
    SystemError::ErrorCode sysErrorCode,
    const nx::network::http::Response* httpResponse)
{
    if (sysErrorCode != SystemError::noError || !httpResponse)
        return ResultCode::networkError;

    if (httpResponse->statusLine.statusCode == nx::network::http::StatusCode::switchingProtocols)
        return ResultCode::ok;

    const auto resultCode = toResultCode(sysErrorCode, httpResponse);
    if (resultCode == api::ResultCode::ok)
    {
        // Server did not upgrade connection, but reported OK. It is unexpected...
        return api::ResultCode::unknownError;
    }
    return resultCode;
}

ResultCode BasicClient::toResultCode(
    SystemError::ErrorCode sysErrorCode,
    const nx::network::http::Response* httpResponse)
{
    if (sysErrorCode != SystemError::noError || !httpResponse)
        return ResultCode::networkError;

    const auto resultCodeStrIter =
        httpResponse->headers.find(Qn::API_RESULT_CODE_HEADER_NAME);
    if (resultCodeStrIter != httpResponse->headers.end())
    {
        return QnLexical::deserialized<api::ResultCode>(
            resultCodeStrIter->second,
            api::ResultCode::unknownError);
    }

    return fromHttpStatusCode(
        static_cast<nx::network::http::StatusCode::Value>(
            httpResponse->statusLine.statusCode));
}

std::string BasicClient::prepareActualRelayUrl(
    const std::string& contentLocationUrl,
    const std::string& requestPath)
{
    auto actualRelayUrl = contentLocationUrl;
    // Removing request path from the end of response.actualRelayUrl
    // so that we have basic relay url.
    NX_ASSERT(
        actualRelayUrl.find(requestPath) != std::string::npos);
    NX_ASSERT(
        actualRelayUrl.find(requestPath) + requestPath.size() ==
        actualRelayUrl.size());

    actualRelayUrl.erase(actualRelayUrl.size() - requestPath.size());

    return actualRelayUrl;
}

void BasicClient::giveFeedback(ResultCode resultCode)
{
    if (m_feedbackFunction)
        m_feedbackFunction(resultCode);
}

} // namespace nx::cloud::relay::api
