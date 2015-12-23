/**********************************************************
* Sep 28, 2015
* akolesnikov
***********************************************************/

#include "auth_provider.h"

#include "cdb_request_path.h"
#include "data/auth_data.h"


namespace nx {
namespace cdb {
namespace cl {

AuthProvider::AuthProvider(network::cloud::CloudModuleEndPointFetcher* const cloudModuleEndPointFetcher)
:
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

}   //cl
}   //cdb
}   //nx
