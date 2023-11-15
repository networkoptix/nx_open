// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "account_manager.h"

#include <nx/branding.h>
#include <nx/network/http/rest/http_rest_client.h>

#include "cdb_request_path.h"

namespace nx::cloud::db::client {

AccountManager::AccountManager(AsyncRequestsExecutor* requestsExecutor):
    m_requestsExecutor(requestsExecutor)
{
}

void AccountManager::registerNewAccount(
    api::AccountRegistrationData accountData,
    std::function<void(api::ResultCode, api::AccountConfirmationCode)> completionHandler)
{
    accountData.customization = nx::branding::customization().toStdString();

    m_requestsExecutor->executeRequest<api::AccountConfirmationCode>(
        nx::network::http::Method::post,
        kAccountRegisterPath,
        std::move(accountData),
        std::move(completionHandler));
}

void AccountManager::activateAccount(
    api::AccountConfirmationCode activationCode,
    std::function<void(api::ResultCode, api::AccountEmail)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::AccountEmail>(
        nx::network::http::Method::post,
        kAccountActivatePath,
        std::move(activationCode),
        std::move(completionHandler));
}

void AccountManager::getAccount(
    std::function<void(api::ResultCode, api::AccountData)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::AccountData>(
        kAccountSelfPath,
        std::move(completionHandler));
}

void AccountManager::getAccountForSharing(
    std::string accountEmail,
    api::AccountForSharingRequest accountRequest,
    std::function<void(api::ResultCode, api::AccountForSharing)> completionHandler)
{
    auto requestPath = nx::network::http::rest::substituteParameters(
        kAccountForSharingPath, {accountEmail});
    m_requestsExecutor->executeRequest<api::AccountForSharing>(
        nx::network::http::Method::get,
        requestPath,
        std::move(accountRequest),
        std::move(completionHandler));
}

void AccountManager::updateAccount(
    api::AccountUpdateData accountData,
    std::function<void(api::ResultCode, api::AccountData)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::AccountData>(
        nx::network::http::Method::put,
        kAccountSelfPath,
        std::move(accountData),
        std::move(completionHandler));
}

void AccountManager::resetPassword(
    api::PasswordResetRequest request,
    std::function<void(api::ResultCode, api::AccountConfirmationCode)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::AccountConfirmationCode>(
        nx::network::http::Method::post,
        kAccountPasswordResetPath,
        std::move(request),
        std::move(completionHandler));
}

void AccountManager::reactivateAccount(
    api::AccountEmail accountEmail,
    std::function<void(
        api::ResultCode,
        api::AccountConfirmationCode)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::AccountConfirmationCode>(
        nx::network::http::Method::post,
        kAccountReactivatePath,
        std::move(accountEmail),
        std::move(completionHandler));
}

void AccountManager::deleteAccount(
    std::function<void(api::ResultCode)> completionHandler)
{
    m_requestsExecutor->executeRequest</*Output*/ void>(
        nx::network::http::Method::delete_,
        nx::network::http::rest::substituteParameters(kAccountPath, {"self"}).c_str(),
        std::move(completionHandler));
}

void AccountManager::createTemporaryCredentials(
    api::TemporaryCredentialsParams params,
    std::function<void(api::ResultCode, api::TemporaryCredentials)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::TemporaryCredentials>(
        nx::network::http::Method::post,
        kAccountCreateTemporaryCredentialsPath,
        std::move(params),
        std::move(completionHandler));
}

void AccountManager::updateSecuritySettings(
    api::AccountSecuritySettings settings,
    std::function<void(api::ResultCode, api::AccountSecuritySettings)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::AccountSecuritySettings>(
        nx::network::http::Method::put,
        nx::network::http::rest::substituteParameters(
            kAccountSecuritySettingsPath, {"self"}).c_str(),
        std::move(settings),
        std::move(completionHandler));
}

void AccountManager::getSecuritySettings(
    std::function<void(api::ResultCode, api::AccountSecuritySettings)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::AccountSecuritySettings>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(
            kAccountSecuritySettingsPath, {"self"}).c_str(),
        std::move(completionHandler));
}

} // nx::cloud::db::client
