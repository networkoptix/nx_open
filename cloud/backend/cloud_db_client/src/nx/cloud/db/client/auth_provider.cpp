#include "auth_provider.h"

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
    executeRequest(
        kAuthGetNoncePath,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::NonceData()));
}

void AuthProvider::getCdbNonce(
    const std::string& systemId,
    std::function<void(api::ResultCode, api::NonceData)> completionHandler)
{
    executeRequest(
        kAuthGetNoncePath,
        api::SystemId(systemId),
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::NonceData()));
}

void AuthProvider::getAuthenticationResponse(
    const api::AuthRequest& authRequest,
    std::function<void(api::ResultCode, api::AuthResponse)> completionHandler)
{
    executeRequest(
        kAuthGetAuthenticationPath,
        authRequest,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::AuthResponse()));
}

void AuthProvider::resolveUserCredentials(
    const api::UserAuthorization& authorization,
    std::function<void(api::ResultCode, api::CredentialsDescriptor)> completionHandler)
{
    executeRequest(
        kAuthResolveUserCredentials,
        authorization,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::CredentialsDescriptor()));
}

void AuthProvider::getSystemAccessLevel(
    const std::string& systemId,
    const api::UserAuthorization& authorization,
    std::function<void(api::ResultCode, api::SystemAccess)> completionHandler)
{
    executeRequest(
        network::http::rest::substituteParameters(kAuthSystemAccessLevel, {systemId}).c_str(),
        authorization,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::SystemAccess()));
}

} // namespace nx::cloud::db::client
