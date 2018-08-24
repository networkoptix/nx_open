/**********************************************************
* Sep 28, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CL_AUTH_PROVIDER_H
#define NX_CDB_CL_AUTH_PROVIDER_H

#include "async_http_requests_executor.h"
#include "include/nx/cloud/cdb/api/auth_provider.h"


namespace nx {
namespace cdb {
namespace client {

class AuthProvider:
    public api::AuthProvider,
    public AsyncRequestsExecutor
{
public:
    AuthProvider(network::cloud::CloudModuleUrlFetcher* const cloudModuleEndPointFetcher);

    virtual void getCdbNonce(
        std::function<void(api::ResultCode, api::NonceData)> completionHandler) override;
    virtual void getCdbNonce(
        const std::string& systemId,
        std::function<void(api::ResultCode, api::NonceData)> completionHandler) override;
    virtual void getAuthenticationResponse(
        const api::AuthRequest& authRequest,
        std::function<void(api::ResultCode, api::AuthResponse)> completionHandler) override;
};


}   //client
}   //cdb
}   //nx

#endif  //NX_CDB_CL_AUTH_PROVIDER_H
