// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include "account_data.h"
#include "result_code.h"

namespace nx::cloud::db::api {

class AccountManager
{
public:
    virtual ~AccountManager() {}

    /**
     * @param accountData. id and statusCode fields are ignored
     *   (assigned by server when creating account).
     * @note Required access role: cloud module (e.g., user portal)
     */
    virtual void registerNewAccount(
        api::AccountRegistrationData accountData,
        std::function<void(api::ResultCode, api::AccountConfirmationCode)> completionHandler) = 0;

    /** Activate account supplying code returned by AccountManager::registerNewAccount. */
    virtual void activateAccount(
        api::AccountConfirmationCode activationCode,
        std::function<void(api::ResultCode, api::AccountEmail)> completionHandler) = 0;

    /**
     * Fetches account info if credentials are account credentials.
     * @note Required access role: account.
     */
    virtual void getAccount(
        std::function<void(api::ResultCode, api::AccountData)> completionHandler) = 0;

    virtual void getAccountForSharing(
        std::string accountEmail,
        api::AccountForSharingRequest accountRequest,
        std::function<void(api::ResultCode, api::AccountForSharing)> completionHandler) = 0;

    virtual void updateAccount(
        api::AccountUpdateData accountData,
        std::function<void(api::ResultCode, api::AccountData)> completionHandler) = 0;

    virtual void resetPassword(
        api::PasswordResetRequest request,
        std::function<void(api::ResultCode, api::AccountConfirmationCode)> completionHandler) = 0;

    /** Requests to re-send account activation code to the email address provided. */
    virtual void reactivateAccount(
        api::AccountEmail accountEmail,
        std::function<void(api::ResultCode, api::AccountConfirmationCode)> completionHandler) = 0;

    /** Deletes account whose credentials are used to authorize the request. */
    virtual void deleteAccount(
        std::function<void(api::ResultCode)> completionHandler) = 0;

    /** Create temporary credentials that can be used instead of account credentials. */
    virtual void createTemporaryCredentials(
        api::TemporaryCredentialsParams params,
        std::function<void(api::ResultCode, api::TemporaryCredentials)> completionHandler) = 0;

    virtual void updateSecuritySettings(
        api::AccountSecuritySettings settings,
        std::function<void(api::ResultCode, api::AccountSecuritySettings)> completionHandler) = 0;

    virtual void getSecuritySettings(
        std::function<void(api::ResultCode, api::AccountSecuritySettings)> completionHandler) = 0;

    virtual void saveAccountOrganizationAttrs(
        const std::string& accountEmail,
        api::AccountOrganizationAttrs orgAttrs,
        std::function<void(api::ResultCode)> completionHandler) = 0;
};

} // namespace nx::cloud::db::api
