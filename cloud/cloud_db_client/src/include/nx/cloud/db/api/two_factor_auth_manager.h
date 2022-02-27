// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include "result_code.h"
#include "two_factor_auth_data.h"

#include <nx/network/http/http_types.h>

namespace nx::cloud::db::api {

class TwoFactorAuthManager
{
public:
    virtual ~TwoFactorAuthManager() = default;

    virtual void generateTotpKey(
        std::function<void(api::ResultCode, api::GenerateKeyResponse)> completionHandler) = 0;

    virtual void generateBackupCodes(
        const api::GenerateBackupCodesRequest& request,
        std::function<void(api::ResultCode, api::BackupCodes)>
            completionHandler) = 0;

    virtual void validateTotpKey(
        const std::string& key,
        const api::ValidateKeyRequest& request,
        std::function<void(api::ResultCode)> completionHandler) = 0;

    virtual void validateBackupCode(
        const std::string& code,
        const api::ValidateBackupCodeRequest& request,
        std::function<void(api::ResultCode)> completionHandler) = 0;

    virtual void deleteBackupCodes(
        const std::string& codes,
        std::function<void(api::ResultCode)> completionHandler) = 0;

    virtual void getBackupCodes(
        std::function<void(api::ResultCode, api::BackupCodes)>
            completionHandler) = 0;
};

} // namespace nx::cloud::db::api
