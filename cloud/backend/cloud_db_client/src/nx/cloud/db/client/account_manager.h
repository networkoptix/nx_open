#pragma once

#include <deque>

#include <QtCore/QUrl>

#include "async_http_requests_executor.h"
#include "data/account_data.h"
#include "include/nx/cloud/db/api/account_manager.h"

namespace nx::cloud::db::client {

class AccountManager:
    public api::AccountManager,
    public AsyncRequestsExecutor
{
public:
    AccountManager(network::cloud::CloudModuleUrlFetcher* cloudModuleUrlFetcher);

    virtual void registerNewAccount(
        api::AccountRegistrationData accountData,
        std::function<void(
            api::ResultCode,
            api::AccountConfirmationCode)> completionHandler) override;
    
    virtual void activateAccount(
        api::AccountConfirmationCode activationCode,
        std::function<void(api::ResultCode, api::AccountEmail)> completionHandler) override;
    
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

} // nx::cloud::db::client
