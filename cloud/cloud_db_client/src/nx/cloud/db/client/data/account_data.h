// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUrlQuery>

#include <stdint.h>
#include <string>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/reflect/instrument.h>

#include <nx/cloud/db/api/account_data.h>

namespace nx::cloud::db::api {

NX_REFLECTION_INSTRUMENT_ENUM(AccountStatus,
    invalid,
    awaitingEmailConfirmation,
    activated,
    blocked,
    invited
)

//-------------------------------------------------------------------------------------------------
// class AccountRegistrationData

//TODO #akolesnikov add corresponding parser/serializer to fusion and remove this function
bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountRegistrationData* const accountData);
void serializeToUrlQuery(const AccountRegistrationData&, QUrlQuery* const urlQuery);

#define AccountRegistrationSecuritySettings_Fields (httpDigestAuthEnabled)

NX_REFLECTION_INSTRUMENT(AccountRegistrationSecuritySettings, AccountRegistrationSecuritySettings_Fields)

#define AccountRegistrationSettings_Fields (security)

NX_REFLECTION_INSTRUMENT(AccountRegistrationSettings, AccountRegistrationSettings_Fields)

QN_FUSION_DECLARE_FUNCTIONS(AccountRegistrationSecuritySettings, (json))
QN_FUSION_DECLARE_FUNCTIONS(AccountRegistrationSettings, (json))

#define AccountRegistrationData_Fields \
    (email)(passwordHa1)(password)\
    (fullName)(customization)(settings)

NX_REFLECTION_INSTRUMENT(AccountRegistrationData, AccountRegistrationData_Fields)

//-------------------------------------------------------------------------------------------------
// class AccountData

#define AccountData_Fields \
    (id)(email)(fullName)(customization)(statusCode)(registrationTime)(activationTime)\
    (httpDigestAuthEnabled)(account2faEnabled)(authSessionLifetime)

NX_REFLECTION_INSTRUMENT(AccountData, AccountData_Fields)

#define AccountStatusData_Fields (statusCode)

NX_REFLECTION_INSTRUMENT(AccountStatusData, AccountStatusData_Fields)
QN_FUSION_DECLARE_FUNCTIONS(AccountStatusData, (json))

//-------------------------------------------------------------------------------------------------
// class AccountConfirmationCode

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountConfirmationCode* const data);
void serializeToUrlQuery(const AccountConfirmationCode&, QUrlQuery* const urlQuery);

#define AccountConfirmationCode_Fields (code)

QN_FUSION_DECLARE_FUNCTIONS(AccountRegistrationData, (json))
QN_FUSION_DECLARE_FUNCTIONS(AccountData, (json)(sql_record))
QN_FUSION_DECLARE_FUNCTIONS(AccountConfirmationCode, (json)(sql_record))

NX_REFLECTION_INSTRUMENT(AccountConfirmationCode, AccountConfirmationCode_Fields)

//-------------------------------------------------------------------------------------------------
// class AccountUpdateData

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountUpdateData* const data);
void serializeToUrlQuery(const AccountUpdateData&, QUrlQuery* const urlQuery);

void serialize(QnJsonContext*, const AccountUpdateData&, QJsonValue*);
bool deserialize(QnJsonContext*, const QJsonValue&, AccountUpdateData*);

#define AccountUpdateData_Fields \
    (passwordHa1)(password)(fullName)(customization)(currentPassword)(totp)(mfaCode)

NX_REFLECTION_INSTRUMENT(AccountUpdateData, AccountUpdateData_Fields)

//-------------------------------------------------------------------------------------------------
// class AccountEmail

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountEmail* const data);
void serializeToUrlQuery(const AccountEmail&, QUrlQuery* const urlQuery);

#define AccountEmail_Fields (email)

QN_FUSION_DECLARE_FUNCTIONS(AccountEmail, (json))
NX_REFLECTION_INSTRUMENT(AccountEmail, AccountEmail_Fields)

//-------------------------------------------------------------------------------------------------
// PasswordResetRequest

#define PasswordResetRequest_Fields (email)(customization)

QN_FUSION_DECLARE_FUNCTIONS(PasswordResetRequest, (json))
NX_REFLECTION_INSTRUMENT(PasswordResetRequest, PasswordResetRequest_Fields)

//-------------------------------------------------------------------------------------------------
// class TemporaryCredentials

bool loadFromUrlQuery(const QUrlQuery& urlQuery, TemporaryCredentialsTimeouts* const data);
void serializeToUrlQuery(const TemporaryCredentialsTimeouts&, QUrlQuery* const urlQuery);

bool loadFromUrlQuery(const QUrlQuery& urlQuery, TemporaryCredentialsParams* const data);
void serializeToUrlQuery(const TemporaryCredentialsParams&, QUrlQuery* const urlQuery);

#define TemporaryCredentialsTimeouts_Fields (expirationPeriod)(autoProlongationEnabled)(prolongationPeriod)

NX_REFLECTION_INSTRUMENT(TemporaryCredentialsTimeouts, TemporaryCredentialsTimeouts_Fields)

#define TemporaryCredentialsParams_Fields (type)(timeouts)

NX_REFLECTION_INSTRUMENT(TemporaryCredentialsParams, TemporaryCredentialsParams_Fields)

#define TemporaryCredentials_Fields (login)(password)(timeouts)

NX_REFLECTION_INSTRUMENT(TemporaryCredentials, TemporaryCredentials_Fields)

QN_FUSION_DECLARE_FUNCTIONS(TemporaryCredentialsTimeouts, (json))
QN_FUSION_DECLARE_FUNCTIONS(TemporaryCredentialsParams, (json))
QN_FUSION_DECLARE_FUNCTIONS(TemporaryCredentials, (json))

//-------------------------------------------------------------------------------------------------
// struct AccountSecuritySettings

QN_FUSION_DECLARE_FUNCTIONS(AccountSecuritySettings, (json))

#define AccountSecuritySettings_Fields \
    (httpDigestAuthEnabled)(password)(account2faEnabled)(totp)(totpExistsForAccount)\
    (authSessionLifetime)(mfaCode)

NX_REFLECTION_INSTRUMENT(AccountSecuritySettings, AccountSecuritySettings_Fields)

//-------------------------------------------------------------------------------------------------
// struct AccountForSharingRequest

void serializeToUrlQuery(const AccountForSharingRequest& requestData, QUrlQuery* urlQuery);
bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountForSharingRequest* const data);

QN_FUSION_DECLARE_FUNCTIONS(AccountForSharingRequest, (json))

#define AccountForSharingRequest_Fields (nonce)

NX_REFLECTION_INSTRUMENT(AccountForSharingRequest, AccountForSharingRequest_Fields)

//-------------------------------------------------------------------------------------------------
// struct AccountForSharing

QN_FUSION_DECLARE_FUNCTIONS(AccountForSharing, (json))

#define AccountForSharing_Fields (id)(email)(fullName)(statusCode)(account2faEnabled)(intermediateResponse)(remote)

NX_REFLECTION_INSTRUMENT(AccountForSharing, AccountForSharing_Fields)

} // namespace nx::cloud::db::api
