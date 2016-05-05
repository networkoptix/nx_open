/**********************************************************
* Sep 25, 2015
* NetworkOptix
* akolesnikov
***********************************************************/

#ifndef NX_CDB_API_AUTH_PROVIDER_H
#define NX_CDB_API_AUTH_PROVIDER_H

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
    //!period, nonce usage should be limited to
    std::chrono::seconds validPeriod;

    NonceData()
    :
        validPeriod(0)
    {
    }
};

class AuthRequest
{
public:
    //!Nonce, reported by \a AuthProvider::getCdbNonce
    std::string nonce;
    //!Username (account email), provided by user
    std::string username;
    //!Realm, we are requesting user digest for
    std::string realm;
};

class AuthResponse
{
public:
    //!nonce from \a AuthRequest
    std::string nonce;
    //!H(ha1:nonce). H is calculated like MD5 but without appending 1 bit, zero bits and message length
    std::string intermediateResponse;
    //!Authorized access role
    SystemAccessRole accessRole;
    //!period, \a intermediateResponse usage should be limited to
    /*!
        This period is introduced to force vms server to verify periodically password at cloud_db
    */
    std::chrono::seconds validPeriod;

    AuthResponse()
    :
        accessRole(SystemAccessRole::none),
        validPeriod(0)
    {
    }
};

//!Provides some temporary hashes which can be used by mediaserver to authenticate requests using cloud account credentials
/*!
    \note These requests are allowed for system only
*/
class AuthProvider
{
public:
    virtual ~AuthProvider() {}

    //!Returns nonce to be used by mediaserver
    virtual void getCdbNonce(
        std::function<void(api::ResultCode, api::NonceData)> completionHandler) = 0;
    //!Returns light_MD5(ha1:nonce). Real digest response can be calculated using this hash
    /*!
        \a light_MD5(ha1:nonce) is calculated by copying A,B,C,D members of MD5_CTX before calling MD5_Final
        Usage of intermediate HTTP Digest response is required to keep ha1 inside cdb.
        \note If \a authRequest.realm value is unknown to CDB, request will fail
    */
    virtual void getAuthenticationResponse(
        const api::AuthRequest& authRequest,
        std::function<void(api::ResultCode, api::AuthResponse)> completionHandler) = 0;
};

}   //api
}   //cdb
}   //nx

#endif  //NX_CDB_API_AUTH_PROVIDER_H
