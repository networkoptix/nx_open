/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CL_CDB_CONNECTION_H
#define NX_CDB_CL_CDB_CONNECTION_H

#include <include/cdb/connection.h>

#include "account_manager.h"
#include "async_http_requests_executor.h"
#include "system_manager.h"
#include "auth_provider.h"


namespace nx {
namespace cc {
    class CloudModuleEndPointFetcher;
}   //cc
}   //nx

namespace nx {
namespace cdb {
namespace cl {

class Connection
:
    public api::Connection,
    public AsyncRequestsExecutor
{
public:
    Connection(
        network::cloud::CloudModuleEndPointFetcher* const endPointFetcher,
        const std::string& login,
        const std::string& password);

    //!Implemetation of api::Connection::getAccountManager
    virtual api::AccountManager* accountManager() override;
    //!Implemetation of api::Connection::getAccountManager
    virtual api::SystemManager* systemManager() override;
    //!Implemetation of api::Connection::authProvider
    virtual api::AuthProvider* authProvider() override;

    //!Implemetation of api::Connection::setCredentials
    virtual void setCredentials(
        const std::string& login,
        const std::string& password) override;
    //!Implemetation of api::Connection::ping
    virtual void ping(
        std::function<void(api::ResultCode, api::ModuleInfo)> completionHandler) override;

private:
    std::unique_ptr<AccountManager> m_accountManager;
    std::unique_ptr<SystemManager> m_systemManager;
    std::unique_ptr<AuthProvider> m_authProvider;
};

}   //cl
}   //cdb
}   //nx

#endif  //NX_CDB_CL_CDB_CONNECTION_H
