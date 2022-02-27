// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

namespace nx::cloud::db::api {

struct GenerateKeyResponse
{
    std::string keyUrl;
};

struct GenerateBackupCodesRequest
{
    int count;
};

struct ValidateKeyRequest
{
    /**%apidoc OAUTH token to validate. */
    std::string token;
};

struct ValidateBackupCodeRequest
{
    std::string token;
};

struct BackupCodeInfo
{
    /**%apidoc The code itself. */
    std::string backup_code;
};

using BackupCodes = std::vector<BackupCodeInfo>;

enum class SecondFactorState
{
    notEntered, //< The token requires the 2nd factor but it wasn't provided.
    entered,
};

} // namespace nx::cloud::db::api
