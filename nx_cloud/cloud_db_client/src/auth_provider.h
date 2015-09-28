/**********************************************************
* Sep 28, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CL_AUTH_PROVIDER_H
#define NX_CDB_CL_AUTH_PROVIDER_H

#include "async_http_requests_executor.h"
#include "include/cdb/auth_provider.h"


namespace nx {
namespace cdb {
namespace cl {

class cc::CloudModuleEndPointFetcher;

class AuthProvider
:
    public api::AuthProvider,
    public AsyncRequestsExecutor
{
public:
    AuthProvider(cc::CloudModuleEndPointFetcher* const cloudModuleEndPointFetcher);

    virtual void getCdbNonce(
        std::function<void(api::ResultCode, api::NonceData)> completionHandler) override;
    virtual void getAuthenticationResponse(
        const api::AuthRequest& authRequest,
        std::function<void(api::ResultCode, api::AuthResponse)> completionHandler) override;
};


}   //cl
}   //cdb
}   //nx

#endif  //NX_CDB_CL_AUTH_PROVIDER_H
