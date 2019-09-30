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

} // namespace nx::cloud::db::client
