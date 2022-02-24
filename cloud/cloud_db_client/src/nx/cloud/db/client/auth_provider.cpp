// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "auth_provider.h"

#include <nx/network/http/http_types.h>
#include <nx/network/http/rest/http_rest_client.h>

#include "cdb_request_path.h"
#include "data/auth_data.h"
#include "data/system_data.h"

namespace nx::cloud::db::client {

AuthProvider::AuthProvider(network::cloud::CloudModuleUrlFetcher* const cloudModuleEndPointFetcher):
    AsyncRequestsExecutor(cloudModuleEndPointFetcher)
{
}

void AuthProvider::getCdbNonce(
    std::function<void(api::ResultCode, api::NonceData)> completionHandler)
{
    executeRequest<api::NonceData>(
        kAuthGetNoncePath,
        std::move(completionHandler));
}

void AuthProvider::getCdbNonce(
    const std::string& systemId,
    std::function<void(api::ResultCode, api::NonceData)> completionHandler)
{
    executeRequest<api::NonceData>(
        nx::network::http::Method::get,
        kAuthGetNoncePath,
        api::SystemId(systemId),
        std::move(completionHandler));
}

void AuthProvider::getAuthenticationResponse(
    const api::AuthRequest& authRequest,
    std::function<void(api::ResultCode, api::AuthResponse)> completionHandler)
{
    executeRequest<api::AuthResponse>(
        nx::network::http::Method::post,
        kAuthGetAuthenticationPath,
        authRequest,
        std::move(completionHandler));
}

template<const char* name>
class HttpHeaderFetcher
{
public:
    using type = std::string;

    type operator()(const nx::network::http::Response& response) const
    {
        return nx::network::http::getHeaderValue(response.headers, name);
    }
};

void AuthProvider::resolveUserCredentials(
    const api::UserAuthorization& authorization,
    std::function<void(api::ResultCode, api::CredentialsDescriptor)> completionHandler)
{
    executeRequest<api::CredentialsDescriptor>(
        nx::network::http::Method::post,
        kAuthResolveUserCredentials,
        authorization,
        std::move(completionHandler));
}

void AuthProvider::resolveUserCredentialsList(
    const api::UserAuthorizationList& authorizationList,
    std::function<void(api::ResultCode, api::CredentialsDescriptorList, std::string)>
        completionHandler)
{
    static constexpr char kCacheControl[] = "Cache-Control";
    executeRequest<api::CredentialsDescriptorList, HttpHeaderFetcher<kCacheControl>>(
        nx::network::http::Method::post,
        kAuthResolveUserCredentialsList,
        authorizationList,
        std::move(completionHandler));
}

void AuthProvider::getSystemAccessLevel(
    const std::string& systemId,
    const api::UserAuthorization& authorization,
    std::function<void(api::ResultCode, api::SystemAccess)> completionHandler)
{
    executeRequest<api::SystemAccess>(
        nx::network::http::Method::post,
        network::http::rest::substituteParameters(kAuthSystemAccessLevel, {systemId}),
        authorization,
        std::move(completionHandler));
}

void AuthProvider::getSystemAccessLevel(
    const std::vector<api::SystemAccessLevelRequest>& requests,
    std::function<void(api::ResultCode, std::vector<api::SystemAccess>)> completionHandler)
{
    struct ResultContext
    {
        std::size_t responsesExpected = 0;
        api::ResultCode resultCode = api::ResultCode::ok;
        std::vector<api::SystemAccess> response;
        std::function<void(api::ResultCode, std::vector<api::SystemAccess>)> completionHandler;
    };

    auto result = std::make_shared<ResultContext>();
    result->responsesExpected = requests.size();
    result->response.resize(requests.size());
    result->completionHandler = std::move(completionHandler);

    post(
        [this, requests, result]()
        {
            for (std::size_t i = 0; i < requests.size(); ++i)
            {
                getSystemAccessLevel(
                    requests[i].systemId, requests[i].authorization,
                    [i, result](api::ResultCode resultCode, api::SystemAccess systemAccess)
                    {
                        if (resultCode != api::ResultCode::ok)
                            result->resultCode = resultCode;
                        result->response[i] = systemAccess;
                        --result->responsesExpected;
                        if (result->responsesExpected == 0)
                        {
                            nx::utils::swapAndCall(
                                result->completionHandler,
                                result->resultCode,
                                std::move(result->response));
                        }
                    });
            }
        });
}

void AuthProvider::getVmsServerTlsPublicKey(
    const std::string& systemId,
    const std::string& serverId,
    const std::string& fingerprint,
    bool isValid,
    std::function<void(api::ResultCode,
        api::VmsServerCertificatePublicKey,
        std::chrono::system_clock::time_point)> completionHandler)
{
    auto completionHandlerWrapper =
        [this, completionHandler = std::move(completionHandler)](
            api::ResultCode res,
            api::VmsServerCertificatePublicKey body,
            std::string validToStr)
    {
        std::chrono::system_clock::time_point validTo =
            std::chrono::system_clock::time_point::min();
        if (res == api::ResultCode::ok
            || res == api::ResultCode::notFound
            || res == api::ResultCode::noContent)
        {
            QDateTime qdt = nx::network::http::parseDate(validToStr);
            if (!qdt.isValid())
            {
                NX_DEBUG(this, "Invalid date-time string received: %1", validToStr);
                completionHandler(api::ResultCode::invalidFormat, {}, validTo);
                return;
            }
            const std::chrono::milliseconds duration{qdt.toMSecsSinceEpoch()};
            validTo = std::chrono::system_clock::time_point{duration};
        }
        completionHandler(res, std::move(body), validTo);
    };
    static constexpr char kExpiresHeader[] = "Expires";
    executeRequest<api::VmsServerCertificatePublicKey, HttpHeaderFetcher<kExpiresHeader>>(
        nx::network::http::Method::get,
        network::http::rest::substituteParameters(
            std::string(kAuthVmsServerCertificatePublicKey)
                + "?valid=" + (isValid ? "1" : "0"),
            {systemId, serverId, fingerprint}),
        std::move(completionHandlerWrapper));
}

} // namespace nx::cloud::db::client
