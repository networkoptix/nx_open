// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "two_factor_auth_manager.h"

#include "cdb_request_path.h"

#include <nx/network/http/rest/http_rest_client.h>

namespace nx::cloud::db::client {

TwoFactorAuthManager::TwoFactorAuthManager(AsyncRequestsExecutor* requestsExecutor):
    m_requestsExecutor(requestsExecutor)
{
}

void TwoFactorAuthManager::generateTotpKey(
    std::function<void(api::ResultCode, api::GenerateKeyResponse)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::GenerateKeyResponse>(
        nx::network::http::Method::post,
        kTwoFactorAuthGetKey,
        std::move(completionHandler));
}

void TwoFactorAuthManager::generateBackupCodes(
    const api::GenerateBackupCodesRequest& request,
    std::function<void(api::ResultCode, api::BackupCodes)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::BackupCodes>(
        nx::network::http::Method::post,
        kTwoFactorAuthBackupCodes,
        request,
        std::move(completionHandler));
}

void TwoFactorAuthManager::validateTotpKey(
    const std::string& key,
    const api::ValidateKeyRequest& request,
    std::function<void(api::ResultCode)> completionHandler)
{
    const auto requestPath =
        nx::network::http::rest::substituteParameters(kTwoFactorAuthValidateKey, {key});

    m_requestsExecutor->executeRequest</*Output*/ void>(
        nx::network::http::Method::get,
        requestPath,
        request,
        std::move(completionHandler));
}

void TwoFactorAuthManager::validateBackupCode(
    const std::string& code,
    const api::ValidateBackupCodeRequest& request,
    std::function<void(api::ResultCode)> completionHandler)
{
    const auto requestPath =
        nx::network::http::rest::substituteParameters(kTwoFactorAuthBackupCode, {code});

    m_requestsExecutor->executeRequest</*Output*/ void>(
        nx::network::http::Method::get,
        requestPath,
        request,
        std::move(completionHandler));
}

void TwoFactorAuthManager::deleteBackupCodes(
    const std::string& codes,
    std::function<void(api::ResultCode)> completionHandler)
{
    const auto requestPath =
        nx::network::http::rest::substituteParameters(kTwoFactorAuthBackupCode, {codes});

    m_requestsExecutor->executeRequest</*Output*/ void>(
        nx::network::http::Method::delete_,
        requestPath,
        std::move(completionHandler));
}

void TwoFactorAuthManager::deleteTotpKey(std::function<void(api::ResultCode)> completionHandler)
{
    m_requestsExecutor->executeRequest</*Output*/ void>(
        nx::network::http::Method::delete_, kTwoFactorAuthGetKey, std::move(completionHandler));
}

void TwoFactorAuthManager::getBackupCodes(
    std::function<void(api::ResultCode, api::BackupCodes)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::BackupCodes>(
        nx::network::http::Method::get,
        kTwoFactorAuthBackupCodes,
        std::move(completionHandler));
}

} // namespace nx::cloud::db::client
