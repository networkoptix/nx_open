// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/cloud/db/api/two_factor_auth_data.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/url_query.h>

namespace nx::cloud::db::api {

void serializeToUrlQuery(const ValidateKeyRequest& requestData, QUrlQuery* urlQuery);
bool loadFromUrlQuery(const QUrlQuery& urlQuery, ValidateKeyRequest* const data);

void serializeToUrlQuery(const ValidateBackupCodeRequest& requestData, QUrlQuery* urlQuery);
bool loadFromUrlQuery(const QUrlQuery& urlQuery, ValidateBackupCodeRequest* const data);

NX_REFLECTION_INSTRUMENT(GenerateKeyResponse, (keyUrl));
NX_REFLECTION_INSTRUMENT(GenerateBackupCodesRequest, (count));
NX_REFLECTION_INSTRUMENT(ValidateKeyRequest, (token));
NX_REFLECTION_INSTRUMENT(ValidateBackupCodeRequest, (token));
NX_REFLECTION_INSTRUMENT(BackupCodeInfo, (backup_code));

bool fromString(const std::string& str, SecondFactorState* state);
std::string toString(SecondFactorState state);

} // namespace nx::cloud::db::api
