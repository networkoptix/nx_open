/**********************************************************
* Sep 28, 2015
* akolesnikov
***********************************************************/

#include "auth_provider.h"

#include "data/auth_data.h"


namespace nx {
namespace cdb {
namespace cl {

AuthProvider::AuthProvider(cc::CloudModuleEndPointFetcher* const cloudModuleEndPointFetcher)
:
    AsyncRequestsExecutor(cloudModuleEndPointFetcher)
{
}

void AuthProvider::getCdbNonce(
    std::function<void(api::ResultCode, api::NonceData)> completionHandler)
{
    executeRequest(
        "/auth/get_cdb_nonce",
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::NonceData()));
}

void AuthProvider::getAuthenticationResponse(
    const api::AuthRequest& authRequest,
    std::function<void(api::ResultCode, api::AuthResponse)> completionHandler)
{
    executeRequest(
        "/auth/authorize",
        authRequest,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::AuthResponse()));
}

}   //cl
}   //cdb
}   //nx
