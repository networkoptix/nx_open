// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

#include <nx/reflect/enum_instrument.h>

namespace nx::cloud::db::api {

struct GenerateKeyResponse
{
    /**%apidoc Should be converted to QR code and used to register with an TOTP authenticator app. */
    std::string keyUrl;
};

struct GenerateBackupCodesRequest
{
    /**%apidoc Number of backup codes to generate. */
    int count = 0;
};

struct ValidateKeyRequest
{
    /**%apidoc OAUTH token to validate. */
    std::string token;
};

struct ValidateBackupCodeRequest
{
    /**%apidoc OAUTH token to validate. */
    std::string token;
};

struct BackupCodeInfo
{
    /**%apidoc The code itself. */
    std::string backup_code;
};

/**%apidoc Array of backup codes. */
using BackupCodes = std::vector<BackupCodeInfo>;

NX_REFLECTION_ENUM_CLASS(SecondFactorState,
    notEntered,
    entered
);

} // namespace nx::cloud::db::api
