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


namespace nx {
namespace cdb {
namespace api {

class NonceData
{
public:
    std::string nonce;
    //!period, nonce usage should be limited to
    std::chrono::seconds validPeriod;
};

class AuthRequest
{
public:
    //!Nonce, reported by \a AuthProvider::getCdbNonce
    std::string nonce;
    std::string username;
    //!Realm, we are requesting user digest for
    std::string realm;
};

class AuthResponse
{
public:
    std::string intermediateResponse;
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
