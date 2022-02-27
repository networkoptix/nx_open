// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "result_code.h"
#include "system_data.h"

namespace nx::cloud::db::api {

class NonceData
{
public:
    std::string nonce;
    /** Period, nonce usage should be limited to. */
    std::chrono::seconds validPeriod;

    NonceData():
        validPeriod(0)
    {
    }
};

class AuthRequest
{
public:
    /** Nonce, reported by AuthProvider::getCdbNonce. */
    std::string nonce;
    /** Username (account email), provided by user. */
    std::string username;
    /** Realm, we are requesting user digest for. */
    std::string realm;
};

class AuthResponse
{
public:
    /** nonce from AuthRequest. */
    std::string nonce;
    /**
     * H(ha1:nonce).
     * H is calculated like MD5 but without appending 1 bit, zero bits and message length.
     * With openssl it can be calculated by copying A,B,C,D members of MD5_CTX before calling MD5_Final.
     */
    std::string intermediateResponse;
    /**
     * Period, intermediateResponse usage should be limited to.
     * This period is introduced to force vms server to verify periodically password at cloud_db.
     */
    std::chrono::seconds validPeriod;
    /** Contains user info and access rights. */
    SystemSharingEx authenticatedAccountData;
    /** Duplicates authenticatedAccountData.accessRole. */
    api::SystemAccessRole accessRole;

    AuthResponse():
        validPeriod(0),
        accessRole(api::SystemAccessRole::none)
    {
    }
};

class UserAuthorization
{
public:
    /**
     * HTTP method from the user request.
     */
    std::string requestMethod;

    /**
     * The "Authorization" header value from the user request. E.g., see [rfc7616] for the spec.
     */
    std::string requestAuthorization;

    std::string baseNonce;
};

class UserAuthorizationList
{
public:
    std::vector<UserAuthorization> authorizations;
};

enum class ObjectType
{
    none = 0,
    account,
    system,
};

class CredentialsDescriptor
{
public:
    api::ResultCode status = api::ResultCode::ok;
    ObjectType objectType = ObjectType::none;

    /**
     * email for account. Id for a system.
     */
    std::string objectId;

    std::string intermediateResponse;
};

class CredentialsDescriptorList
{
public:
    std::vector<CredentialsDescriptor> descriptors;
};

class SystemAccess
{
public:
    SystemAccessRole accessRole = SystemAccessRole::none;
};

class SystemAccessLevelRequest
{
public:
    std::string systemId;
    api::UserAuthorization authorization;
};

struct VmsServerCertificatePublicKey
{
    /**%apidoc
     * Public key from a VMS Server certificate (PEM).
     */
    std::string key;
};

/**
 * Provides some temporary hashes which can be used by mediaserver
 *   to authenticate requests using cloud account credentials.
 * NOTE: These requests are allowed for system only.
 */
class AuthProvider
{
public:
    virtual ~AuthProvider() = default;

    /**
     * @return nonce to be used by mediaserver.
     */
    virtual void getCdbNonce(
        std::function<void(api::ResultCode, api::NonceData)> completionHandler) = 0;

    virtual void getCdbNonce(
        const std::string& systemId,
        std::function<void(api::ResultCode, api::NonceData)> completionHandler) = 0;

    /**
     * NOTE: If authRequest.realm value is unknown to CDB, request will fail.
     */
    virtual void getAuthenticationResponse(
        const api::AuthRequest& authRequest,
        std::function<void(api::ResultCode, api::AuthResponse)> completionHandler) = 0;

    virtual void resolveUserCredentials(
        const api::UserAuthorization& authorization,
        std::function<void(api::ResultCode, api::CredentialsDescriptor)> completionHandler) = 0;

    virtual void resolveUserCredentialsList(
        const api::UserAuthorizationList& authorizationList,
        std::function<void(api::ResultCode, api::CredentialsDescriptorList, std::string)>
            completionHandler) = 0;

    virtual void getSystemAccessLevel(
        const std::string& systemId,
        const api::UserAuthorization& authorization,
        std::function<void(api::ResultCode, api::SystemAccess)> completionHandler) = 0;

    /**
     * @param completionHandler The order of system access corresponds to that of requests.
     */
    virtual void getSystemAccessLevel(
        const std::vector<api::SystemAccessLevelRequest>& requests,
        std::function<void(api::ResultCode, std::vector<api::SystemAccess>)> completionHandler) = 0;

    virtual void getVmsServerTlsPublicKey(
        const std::string& systemId,
        const std::string& serverId,
        const std::string& fingerprint,
        bool isValid,
        std::function<void(api::ResultCode, VmsServerCertificatePublicKey,
            std::chrono::system_clock::time_point)> completionHandler) = 0;
};

//-------------------------------------------------------------------------------------------------

class AuthInfoRecord
{
public:
    AuthInfoRecord(
        const std::string& nonce,
        const std::string& intermediateResponse,
        std::chrono::system_clock::time_point expirationTime)
        :
        nonce(nonce),
        intermediateResponse(intermediateResponse),
        expirationTime(expirationTime)
    {}

    AuthInfoRecord() = default;

    std::string nonce;
    /**
     * See AuthResponse::intermediateResponse.
     */
    std::string intermediateResponse;
    std::chrono::system_clock::time_point expirationTime;

    bool operator==(const AuthInfoRecord& right) const
    {
        return nonce == right.nonce
            && intermediateResponse == right.intermediateResponse
            && expirationTime == right.expirationTime;
    }
};

class AuthInfo
{
public:
    std::vector<AuthInfoRecord> records;

    bool operator==(const AuthInfo& right) const
    {
        return records == right.records;
    }

    bool operator!=(const AuthInfo& right) const
    {
        return records != right.records;
    }
};

class SystemNonce
{
public:
    std::string systemId;
    std::string nonce;
};

class SystemUserAuthInfoRecord : public AuthInfoRecord
{
public:
    SystemUserAuthInfoRecord(const AuthInfoRecord& base) : AuthInfoRecord(base)
    {}
    SystemUserAuthInfoRecord() = default;

    std::string systemId;
    std::string accountId;
};

} // namespace nx::cloud::db::api
