// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "two_factor_auth_manager.h"

#include "cdb_request_path.h"

#include <nx/network/http/rest/http_rest_client.h>

namespace nx::cloud::db::client {

TwoFactorAuthManager::TwoFactorAuthManager(ApiRequestsExecutor* requestsExecutor):
    m_requestsExecutor(requestsExecutor)
{
}

void TwoFactorAuthManager::generateTotpKey(
    std::function<void(api::ResultCode, api::GenerateKeyResponse)> completionHandler)
{
    m_requestsExecutor->makeAsyncCall<api::GenerateKeyResponse>(
        nx::network::http::Method::post,
        kTwoFactorAuthGetKey,
        {}, //query
        std::move(completionHandler));
}

void TwoFactorAuthManager::generateBackupCodes(
    const api::GenerateBackupCodesRequest& request,
    std::function<void(api::ResultCode, api::BackupCodes)> completionHandler)
{
    m_requestsExecutor->makeAsyncCall<api::BackupCodes>(
        nx::network::http::Method::post,
        kTwoFactorAuthBackupCodes,
        {}, //query
        request,
        std::move(completionHandler));
}

void TwoFactorAuthManager::validateTotpKey(
    const std::string& key,
    const api::ValidateKeyRequest& request,
    std::function<void(api::ResultCode)> completionHandler)
{
    nx::utils::UrlQuery query;
    query.addQueryItem("token", request.token);

    m_requestsExecutor->makeAsyncCall</*Output*/ void>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(kTwoFactorAuthValidateKey, {key}),
        query,
        std::move(completionHandler));
}

void TwoFactorAuthManager::validateBackupCode(
    const std::string& code,
    const api::ValidateBackupCodeRequest& request,
    std::function<void(api::ResultCode)> completionHandler)
{
    nx::utils::UrlQuery query;
    query.addQueryItem("token", request.token);

    m_requestsExecutor->makeAsyncCall</*Output*/ void>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(kTwoFactorAuthBackupCode, {code}),
        query,
        std::move(completionHandler));
}

void TwoFactorAuthManager::deleteBackupCodes(
    const std::string& codes,
    std::function<void(api::ResultCode)> completionHandler)
{
    m_requestsExecutor->makeAsyncCall</*Output*/ void>(
        nx::network::http::Method::delete_,
        nx::network::http::rest::substituteParameters(kTwoFactorAuthBackupCode, {codes}),
        {}, //query
        std::move(completionHandler));
}

void TwoFactorAuthManager::deleteTotpKey(std::function<void(api::ResultCode)> completionHandler)
{
    m_requestsExecutor->makeAsyncCall</*Output*/ void>(
        nx::network::http::Method::delete_,
        kTwoFactorAuthGetKey,
        {}, //query
        std::move(completionHandler));
}

void TwoFactorAuthManager::getBackupCodes(
    std::function<void(api::ResultCode, api::BackupCodes)> completionHandler)
{
    m_requestsExecutor->makeAsyncCall<api::BackupCodes>(
        nx::network::http::Method::get,
        kTwoFactorAuthBackupCodes,
        {}, //query
        std::move(completionHandler));
}

} // namespace nx::cloud::db::client
