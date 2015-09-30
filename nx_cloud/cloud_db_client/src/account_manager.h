/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CL_ACCOUNT_MANAGER_H
#define NX_CDB_CL_ACCOUNT_MANAGER_H

#include <deque>

#include <QtCore/QUrl>

#include "async_http_requests_executor.h"
#include "data/account_data.h"
#include "include/cdb/account_manager.h"


namespace nx {
namespace cdb {
namespace cl {

class cc::CloudModuleEndPointFetcher;

class AccountManager
:
    public api::AccountManager,
    public AsyncRequestsExecutor
{
public:
    AccountManager(cc::CloudModuleEndPointFetcher* const cloudModuleEndPointFetcher);

    //!Implementation of api::AccountManager::registerNewAccount
    virtual void registerNewAccount(
        const api::AccountData& accountData,
        std::function<void(api::ResultCode, api::AccountActivationCode)> completionHandler) override;
    //!Implementation of api::AccountManager::activateAccount
    virtual void activateAccount(
        const api::AccountActivationCode& activationCode,
        std::function<void(api::ResultCode)> completionHandler) override;
    //!Implementation of api::AccountManager::getAccount
    virtual void getAccount(
        std::function<void(api::ResultCode, api::AccountData)> completionHandler) override;
};


}   //cl
}   //cdb
}   //nx

#endif  //NX_CDB_CL_ACCOUNT_MANAGER_H
