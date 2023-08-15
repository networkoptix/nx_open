// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "async_http_requests_executor.h"
#include "data/two_factor_auth_data.h"
#include "include/nx/cloud/db/api/two_factor_auth_manager.h"

namespace nx::cloud::db::client {

class TwoFactorAuthManager:
    public api::TwoFactorAuthManager
{
public:
    TwoFactorAuthManager(AsyncRequestsExecutor* requestsExecutor);

    virtual void generateTotpKey(
        std::function<void(api::ResultCode, api::GenerateKeyResponse)> completionHandler) override;

    virtual void generateBackupCodes(
        const api::GenerateBackupCodesRequest& request,
        std::function<void(api::ResultCode, api::BackupCodes)> completionHandler)
        override;

    virtual void validateTotpKey(
        const std::string& key,
        const api::ValidateKeyRequest& request,
        std::function<void(api::ResultCode)> completionHandler) override;

    virtual void validateBackupCode(
        const std::string& code,
        const api::ValidateBackupCodeRequest& request,
        std::function<void(api::ResultCode)> completionHandler) override;

    virtual void deleteBackupCodes(
        const std::string& codes, std::function<void(api::ResultCode)> completionHandler) override;

    virtual void deleteTotpKey(std::function<void(api::ResultCode)> completionHandler) override;

    virtual void getBackupCodes(
        std::function<void(api::ResultCode, api::BackupCodes)> completionHandler) override;

private:
    AsyncRequestsExecutor* m_requestsExecutor = nullptr;
};

} // namespace nx::cloud::db::client
