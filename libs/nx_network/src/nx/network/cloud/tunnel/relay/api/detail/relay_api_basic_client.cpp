// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "relay_api_basic_client.h"

#include <boost/algorithm/string/predicate.hpp>

#include <nx/network/http/custom_headers.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/string.h>
#include <nx/utils/type_utils.h>

#include "../relay_api_http_paths.h"

namespace nx::cloud::relay::api::detail {

BasicClient::BasicClient(const nx::utils::Url& baseUrl):
    m_baseUrl(baseUrl)
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

nx::network::http::StatusCode::Value BasicClient::prevRequestHttpStatusCode() const
{
    return m_prevHttpStatusCode;
}

void BasicClient::setTimeout(std::optional<std::chrono::milliseconds> timeout)
{
    m_timeout = timeout;
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
        return toResultCode(sysErrorCode);

    const auto resultCodeStrIter =
        httpResponse->headers.find(Qn::API_RESULT_CODE_HEADER_NAME);
    if (resultCodeStrIter != httpResponse->headers.end())
        return nx::reflect::fromString(resultCodeStrIter->second, api::ResultCode::unknownError);

    return fromHttpStatusCode(
        static_cast<nx::network::http::StatusCode::Value>(
            httpResponse->statusLine.statusCode));
}

ResultCode BasicClient::toResultCode(
    SystemError::ErrorCode sysErrorCode)
{
    switch (sysErrorCode)
    {
        case SystemError::noError:
            return ResultCode::ok;

        case SystemError::timedOut:
            return ResultCode::timedOut;

        default:
            return ResultCode::networkError;
    }
}

std::string BasicClient::prepareActualRelayUrl(
    const std::string& contentLocationUrl,
    const std::string& requestPath)
{
    auto actualRelayUrl = contentLocationUrl;

    if (!nx::utils::endsWith(actualRelayUrl, requestPath))
    {
        NX_DEBUG(this, "Relay URL %1 does not end with expected path %2. A compatibility issue "
            "is likely", actualRelayUrl, requestPath);
        return actualRelayUrl; //< In worst case, a request will fail.
    }

    // Removing request path from the end of response.actualRelayUrl
    // so that we have basic relay url.
    actualRelayUrl.erase(actualRelayUrl.size() - requestPath.size());

    return actualRelayUrl;
}

} // namespace nx::cloud::relay::api::detail
