// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "result_code.h"
#include "system_data.h"

namespace nx::cloud::db::api {

struct NonceData
{
    /**%apidoc Nonce specific to requested system. */
    std::string nonce;

    /**%apidoc Period, nonce usage should be limited to. */
    std::chrono::seconds validPeriod = std::chrono::seconds::zero();
};

struct AuthRequest
{
    /**%apidoc Nonce, reported by AuthProvider::getCdbNonce. */
    std::string nonce;

    /**%apidoc Username (account email), provided by user. */
    std::string username;

    /**%apidoc Realm, we are requesting user digest for. */
    std::string realm;
};

struct AuthResponse
{
    /**%apidoc nonce from AuthRequest. */
    std::string nonce;

    /**%apidoc
     * H(ha1:nonce).
     * H is calculated like MD5 but without appending 1 bit, zero bits and message length.
     * With openssl it can be calculated by copying A,B,C,D members of MD5_CTX before calling MD5_Final.
     */
    std::string intermediateResponse;

    /**%apidoc Period, intermediateResponse usage should be limited to.
     * This period is introduced to force vms server to verify periodically password at cloud_db.
     */
    std::chrono::seconds validPeriod = std::chrono::seconds::zero();

    /**%apidoc Contains user info and access rights. */
    SystemSharing authenticatedAccountData;

    /**%apidoc Duplicates authenticatedAccountData.accessRole. */
    api::SystemAccessRole accessRole = api::SystemAccessRole::none;
};

struct UserAuthorization
{
    /**%apidoc HTTP method from the user request. */
    std::string requestMethod;

    /**%apidoc The "Authorization" header value from the user request.
     * Basic, Digest, Bearer HTTP authorization types are supported.
     */
    std::string requestAuthorization;

    /**%apidoc Nonce, specific for a system. */
    std::string baseNonce;
};

struct UserAuthorizationList
{
    std::vector<UserAuthorization> authorizations;
};

enum class ObjectType
{
    none = 0,
    account,
    system,
};

struct CredentialsDescriptor
{
    /**%apidoc Credentials resolve result code. */
    api::ResultCode status = api::ResultCode::ok;

    ObjectType objectType = ObjectType::none;

    /**%apidoc Id of resolved object. Email for an account. Id for a system. */
    std::string objectId;

    /**%apidoc MD5(HA1 ":" baseNonce). Present only if baseNonce was present in the request. */
    std::string intermediateResponse;
};

struct CredentialsDescriptorList
{
    std::vector<CredentialsDescriptor> descriptors;
};

struct SystemAccess
{
    /**%apidoc The access level for a given system. */
    SystemAccessRole accessRole = SystemAccessRole::none;
};

struct SystemAccessLevelRequest
{
    std::string systemId;
    api::UserAuthorization authorization;
};

struct VmsServerCertificatePublicKey
{
    /**%apidoc Public key from a VMS Server certificate (PEM). */
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

struct AuthInfo
{
    std::vector<AuthInfoRecord> records;
    bool twofaEnabled = false;

    bool operator==(const AuthInfo& right) const
    {
        return records == right.records;
    }

    bool operator!=(const AuthInfo& right) const
    {
        return records != right.records;
    }
};

struct SystemNonce
{
    std::string systemId;
    std::string nonce;
};

} // namespace nx::cloud::db::api
