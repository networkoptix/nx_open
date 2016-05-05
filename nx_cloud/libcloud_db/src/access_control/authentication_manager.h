/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_authentication_manager_h
#define cloud_db_authentication_manager_h

#include <functional>
#include <random>

#include <nx/network/http/server/abstract_authentication_manager.h>

#include <cdb/result_code.h>

#include "auth_types.h"


class QnAuthMethodRestrictionList;

namespace nx {
namespace cdb {

class AccountManager;
class SystemManager;
class TemporaryAccountPasswordManager;
class StreeManager;
class AbstractAuthenticationDataProvider;

//!Performs authentication based on various parameters
/*!
    Typically, username/digest is used to authenticate, but IP address/network interface and other data can be used also.
    Uses account data and some predefined static data to authenticate incoming requests.
    \note Listens to user data change events
 */
class AuthenticationManager
:
    public nx_http::AbstractAuthenticationManager
{
public:
    AuthenticationManager(
        std::vector<AbstractAuthenticationDataProvider*> authDataProviders,
        const QnAuthMethodRestrictionList& authRestrictionList,
        const StreeManager& stree);

    virtual bool authenticate(
        const nx_http::HttpServerConnection& connection,
        const nx_http::Request& request,
        boost::optional<nx_http::header::WWWAuthenticate>* const wwwAuthenticate,
        stree::ResourceContainer* authProperties,
        nx_http::HttpHeaders* const responseHeaders,
        std::unique_ptr<nx_http::AbstractMsgBodySource>* const msgBody) override;
        
    static nx::String realm(); 

private:
    const QnAuthMethodRestrictionList& m_authRestrictionList;
    const StreeManager& m_stree;
    std::random_device m_rd;
    std::uniform_int_distribution<size_t> m_dist;
    std::vector<AbstractAuthenticationDataProvider*> m_authDataProviders;

    bool validateNonce(const nx_http::StringType& nonce);
    api::ResultCode authenticateInDataManagers(
        const nx_http::StringType& username,
        std::function<bool(const nx::Buffer&)> validateHa1Func,
        const stree::AbstractResourceReader& authSearchInputData,
        stree::ResourceContainer* const authProperties);
    void addWWWAuthenticateHeader(
        boost::optional<nx_http::header::WWWAuthenticate>* const wwwAuthenticate );
    nx::Buffer generateNonce();
};

}   //cdb
}   //nx

#endif
