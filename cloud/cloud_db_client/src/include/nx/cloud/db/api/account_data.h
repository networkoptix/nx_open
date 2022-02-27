// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>

namespace nx::cloud::db::api {

enum class AccountStatus
{
    invalid = 0,
    awaitingEmailConfirmation = 1,
    activated = 2,
    blocked = 3,
    invited = 4,
};

struct AccountRegistrationSecuritySettings
{
    std::optional<bool> httpDigestAuthEnabled;
};

struct AccountRegistrationSettings
{
    AccountRegistrationSecuritySettings security;
};

class AccountRegistrationData
{
public:
    /** User email. Used as unique user id. */
    std::string email;
    /** Hex representation of HA1 (see rfc2617) digest of user's password. */
    std::string passwordHa1;
    std::string password;
    std::string fullName;
    std::string customization;
    AccountRegistrationSettings settings;
};

enum class PasswordHashType
{
    invalid = 0,
    ha1md5 = 1,
    scrypt = 2,
};

struct PasswordHash
{
    std::string hash;
    std::unordered_map<std::string, std::string> attributes;
};

struct AccountData
{
    /**%apidoc[readonly] Account id. */
    std::string id;
    /**%apidoc[readonly] User email. Unique for each account. */
    std::string email;
    std::unordered_map<PasswordHashType, PasswordHash> passwordHashes;
    std::string fullName;
    std::string customization;
    AccountStatus statusCode = AccountStatus::invalid;
    std::chrono::system_clock::time_point registrationTime;
    /**
     * Zero, if account has not been activated yet.
     */
    std::chrono::system_clock::time_point activationTime;
    bool httpDigestAuthEnabled = true;
    bool account2faEnabled = false;
};

class AccountConfirmationCode
{
public:
    std::string code;
};

class AccountUpdateData
{
public:
    std::optional<std::string> passwordHa1;
    std::optional<std::string> password;
    std::optional<std::string> fullName;
    std::optional<std::string> customization;
};

class AccountEmail
{
public:
    std::string email;
};

class TemporaryCredentialsTimeouts
{
public:
    /**%apidoc Expiration timeout.
     * When automatic prolongation is enabled this period works a bit different:
     * credentials expire if not used for this timeout.
     */
    std::chrono::seconds expirationPeriod = std::chrono::seconds::zero();

    /**%apidoc Enable/disable automatic credentials prolongation.
     * if true, then credentials expiration time is set to currentTime() + prolongationPeriod
     * on each request.
     */
    bool autoProlongationEnabled = false;

    /**%apidoc This value is use to prolongate credentials.
     */
    std::chrono::seconds prolongationPeriod = std::chrono::seconds::zero();
};

class TemporaryCredentialsParams
{
public:
    /**%apidoc Type of key to issue.
     * The following types are defined:
     * - short
     * - long
     * If empty, then timeouts field has to be specified.
     */
    std::string type;

    /**%apidoc Custom expiration settings.
     * Ignored if type is non-empty.
     */
    TemporaryCredentialsTimeouts timeouts;
};

class TemporaryCredentials
{
public:
    /**%apidoc Temporary login.
     * Can only be used with the password from the same response.
     */
    std::string login;

    /**%apidoc Temporary password.
     * Can only be used with the login from the same response.
     */
    std::string password;

    /**%apidoc Expiration parameters of the credentials. */
    TemporaryCredentialsTimeouts timeouts;
};

struct AccountSecuritySettings
{
    /**%apidoc Enable/disable HTTP Digest authentication for the account.
     * MUST be accompanied by password.
     */
    std::optional<bool> httpDigestAuthEnabled;

    /**%apidoc Enable/disable two-factor authentication for the account.
     * MUST be accompanied by valid totp field.
     */
    std::optional<bool> account2faEnabled;

    /**%apidoc One-time password from the authenicator app.
     * Required and MUST be valid if account2faEnabled is specified.
     */
    std::optional<std::string> totp;

    /**%apidoc Account password. Always required. */
    std::optional<std::string> password;
};

struct AccountForSharingRequest
{
    std::string nonce;
};

struct AccountForSharing
{
    std::string id;
    std::string email;
    std::string fullName;
    AccountStatus statusCode = AccountStatus::invalid;
    bool account2faEnabled = false;
    std::string intermediateResponse;
};

} // namespace nx::cloud::db::api
