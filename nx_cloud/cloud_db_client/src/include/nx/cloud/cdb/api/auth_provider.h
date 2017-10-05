#pragma once

#include <chrono>
#include <functional>
#include <string>

#include "result_code.h"
#include "system_data.h"

namespace nx {
namespace cdb {
namespace api {

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

/**
 * Provides some temporary hashes which can be used by mediaserver
 *   to authenticate requests using cloud account credentials.
 * NOTE: These requests are allowed for system only.
 */
class AuthProvider
{
public:
    virtual ~AuthProvider() {}

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
};

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
};

} // namespace api
} // namespace cdb
} // namespace nx
