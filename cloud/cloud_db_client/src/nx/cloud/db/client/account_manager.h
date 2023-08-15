// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>

#include <QtCore/QUrl>

#include "async_http_requests_executor.h"
#include "data/account_data.h"
#include "include/nx/cloud/db/api/account_manager.h"

namespace nx::cloud::db::client {

class AccountManager:
    public api::AccountManager
{
public:
    AccountManager(AsyncRequestsExecutor* requestsExecutor);

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

    virtual void getAccountForSharing(
        std::string accountEmail,
        api::AccountForSharingRequest accountRequest,
        std::function<void(api::ResultCode, api::AccountForSharing)> completionHandler) override;

    virtual void updateAccount(
        api::AccountUpdateData accountData,
        std::function<void(api::ResultCode, api::AccountData)> completionHandler) override;

    virtual void resetPassword(
        api::PasswordResetRequest request,
        std::function<void(
            api::ResultCode,
            api::AccountConfirmationCode)> completionHandler) override;

    virtual void reactivateAccount(
        api::AccountEmail accountEmail,
        std::function<void(
            api::ResultCode,
            api::AccountConfirmationCode)> completionHandler) override;

    virtual void deleteAccount(
        std::function<void(api::ResultCode)> completionHandler) override;

    virtual void createTemporaryCredentials(
        api::TemporaryCredentialsParams params,
        std::function<void(api::ResultCode, api::TemporaryCredentials)> completionHandler) override;

    virtual void updateSecuritySettings(
        api::AccountSecuritySettings settings,
        std::function<void(api::ResultCode, api::AccountSecuritySettings)> completionHandler) override;

    virtual void getSecuritySettings(
        std::function<void(api::ResultCode, api::AccountSecuritySettings)> completionHandler) override;

private:
    AsyncRequestsExecutor* m_requestsExecutor = nullptr;
};

} // nx::cloud::db::client
