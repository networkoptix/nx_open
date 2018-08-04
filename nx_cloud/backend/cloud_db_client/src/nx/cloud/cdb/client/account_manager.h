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
#include "include/nx/cloud/cdb/api/account_manager.h"


namespace nx {
namespace cdb {
namespace client {

class AccountManager:
    public api::AccountManager,
    public AsyncRequestsExecutor
{
public:
    AccountManager(network::cloud::CloudModuleUrlFetcher* const cloudModuleEndPointFetcher);

    //!Implementation of api::AccountManager::registerNewAccount
    virtual void registerNewAccount(
        api::AccountRegistrationData accountData,
        std::function<void(
            api::ResultCode,
            api::AccountConfirmationCode)> completionHandler) override;
    //!Implementation of api::AccountManager::activateAccount
    virtual void activateAccount(
        api::AccountConfirmationCode activationCode,
        std::function<void(api::ResultCode, api::AccountEmail)> completionHandler) override;
    //!Implementation of api::AccountManager::getAccount
    virtual void getAccount(
        std::function<void(api::ResultCode, api::AccountData)> completionHandler) override;
    virtual void updateAccount(
        api::AccountUpdateData accountData,
        std::function<void(api::ResultCode)> completionHandler) override;
    virtual void resetPassword(
        api::AccountEmail accountEmail,
        std::function<void(
            api::ResultCode,
            api::AccountConfirmationCode)> completionHandler) override;
    virtual void reactivateAccount(
        api::AccountEmail accountEmail,
        std::function<void(
            api::ResultCode,
            api::AccountConfirmationCode)> completionHandler) override;
    virtual void createTemporaryCredentials(
        api::TemporaryCredentialsParams params,
        std::function<void(api::ResultCode, api::TemporaryCredentials)> completionHandler) override;
};

}   //client
}   //cdb
}   //nx

#endif  //NX_CDB_CL_ACCOUNT_MANAGER_H
