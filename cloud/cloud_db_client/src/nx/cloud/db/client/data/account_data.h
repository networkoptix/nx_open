// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>
#include <string>

#include <QtCore/QUrlQuery>

#include <nx/cloud/db/api/account_data.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

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

#define AccountRegistrationData_Fields \
    (email)(passwordHa1)(password)\
    (fullName)(customization)(settings)

NX_REFLECTION_INSTRUMENT(AccountRegistrationData, AccountRegistrationData_Fields)

//-------------------------------------------------------------------------------------------------
// class AccountData

#define AccountData_Fields \
    (id)(email)(fullName)(customization)(statusCode)(registrationTime)(activationTime)\
    (httpDigestAuthEnabled)(account2faEnabled)(authSessionLifetime)(accountBelongsToOrganization)\
    (securitySequence)(accountLocation)(locale)

NX_REFLECTION_INSTRUMENT(AccountData, AccountData_Fields)

NX_REFLECTION_INSTRUMENT(AccountStatusData, (statusCode))

//-------------------------------------------------------------------------------------------------
// class AccountConfirmationCode

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountConfirmationCode* const data);
void serializeToUrlQuery(const AccountConfirmationCode&, QUrlQuery* const urlQuery);

#define AccountConfirmationCode_Fields (code)

NX_REFLECTION_INSTRUMENT(AccountConfirmationCode, AccountConfirmationCode_Fields)

//-------------------------------------------------------------------------------------------------
// class AccountUpdateData

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountUpdateData* const data);
void serializeToUrlQuery(const AccountUpdateData&, QUrlQuery* const urlQuery);

#define AccountUpdateData_Fields \
    (passwordHa1)(password)(fullName)(customization)(currentPassword)(totp)(mfaCode)(locale)

NX_REFLECTION_INSTRUMENT(AccountUpdateData, AccountUpdateData_Fields)

//-------------------------------------------------------------------------------------------------
// class AccountOrganizationAttrs

#define AccountOrganizationAttrs_Fields (belongsToSomeOrganization)

NX_REFLECTION_INSTRUMENT(AccountOrganizationAttrs, AccountOrganizationAttrs_Fields)

//-------------------------------------------------------------------------------------------------
// class AccountEmail

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountEmail* const data);
void serializeToUrlQuery(const AccountEmail&, QUrlQuery* const urlQuery);

#define AccountEmail_Fields (email)

NX_REFLECTION_INSTRUMENT(AccountEmail, AccountEmail_Fields)

//-------------------------------------------------------------------------------------------------
// PasswordResetRequest

#define PasswordResetRequest_Fields (email)(customization)

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

//-------------------------------------------------------------------------------------------------
// struct AccountSecuritySettings

#define AccountSecuritySettings_Fields \
    (httpDigestAuthEnabled)(password)(account2faEnabled)(totp)(totpExistsForAccount)\
    (authSessionLifetime)(mfaCode)

NX_REFLECTION_INSTRUMENT(AccountSecuritySettings, AccountSecuritySettings_Fields)

//-------------------------------------------------------------------------------------------------
// struct AccountForSharingRequest

void serializeToUrlQuery(const AccountForSharingRequest& requestData, QUrlQuery* urlQuery);
bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountForSharingRequest* const data);

#define AccountForSharingRequest_Fields (nonce)

NX_REFLECTION_INSTRUMENT(AccountForSharingRequest, AccountForSharingRequest_Fields)

//-------------------------------------------------------------------------------------------------
// struct AccountForSharing

#define AccountForSharing_Fields (id)(email)(fullName)(statusCode)(account2faEnabled)(intermediateResponse)(remote)

NX_REFLECTION_INSTRUMENT(AccountForSharing, AccountForSharing_Fields)

} // namespace nx::cloud::db::api
