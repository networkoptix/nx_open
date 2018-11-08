#pragma once

#include <cstdint>
#include <chrono>
#include <string>

#include <boost/optional.hpp>

namespace nx {
namespace cdb {
namespace api {

enum class AccountStatus
{
    invalid = 0,
    awaitingActivation = 1,
    activated = 2,
    blocked = 3,
    invited = 4,
};

class AccountRegistrationData
{
public:
    /** User email. Used as unique user id. */
    std::string email;
    /** Hex representation of HA1 (see rfc2617) digest of user's password. */
    std::string passwordHa1;
    /** Hex representation of HA1 (see rfc2617) digest calculated with SHA-256 hash. */
    std::string passwordHa1Sha256;
    std::string fullName;
    std::string customization;
};

class AccountData
{
public:
    std::string id;
    /** User email. Used as unique user id. */
    std::string email;
    /** Hex representation of HA1 (see rfc2617) digest of user's password. */
    std::string passwordHa1;
    /** Hex representation of HA1 (see rfc2617) digest calculated with SHA-256 hash. */
    std::string passwordHa1Sha256;
    std::string fullName;
    std::string customization;
    AccountStatus statusCode;
    std::chrono::system_clock::time_point registrationTime;
    /**
     * Zero, if account has not been activated yet.
     */
    std::chrono::system_clock::time_point activationTime;

    AccountData():
        AccountData(AccountRegistrationData())
    {
    }

    AccountData(AccountRegistrationData registrationData):
        email(std::move(registrationData.email)),
        passwordHa1(std::move(registrationData.passwordHa1)),
        passwordHa1Sha256(std::move(registrationData.passwordHa1Sha256)),
        fullName(std::move(registrationData.fullName)),
        customization(std::move(registrationData.customization)),
        statusCode(AccountStatus::invalid)
    {
    }
};

class AccountConfirmationCode
{
public:
    std::string code;
};

class AccountUpdateData
{
public:
    boost::optional<std::string> passwordHa1;
    boost::optional<std::string> fullName;
    boost::optional<std::string> customization;
    /** Hex representation of HA1 (see rfc2617) digest calculated with SHA-256 hash. */
    boost::optional<std::string> passwordHa1Sha256;
};

class AccountEmail
{
public:
    std::string email;
};

class TemporaryCredentialsTimeouts
{
public:
    std::chrono::seconds expirationPeriod;
    /**
     * if true, each request, authorized with these credentials, 
     * increases credentials lifetime by prolongationPeriod.
     */
    bool autoProlongationEnabled;
    std::chrono::seconds prolongationPeriod;

    TemporaryCredentialsTimeouts():
        expirationPeriod(0),
        autoProlongationEnabled(false),
        prolongationPeriod(0)
    {
    }
};

/**
 * Input parameters of create_temporary_credentials request.
 */
class TemporaryCredentialsParams
{
public:
    /**
     * Type of key to issue.
     * Following types are defined:
     * - short
     * - long
     */
    std::string type;
    /**
     * This structure is ignored if type is non-empty.
     */
    TemporaryCredentialsTimeouts timeouts;
};

class TemporaryCredentials
{
public:
    std::string login;
    std::string password;
    /** Parameters of credentials created. */
    TemporaryCredentialsTimeouts timeouts;
};

} // namespace api
} // namespace cdb
} // namespace nx
