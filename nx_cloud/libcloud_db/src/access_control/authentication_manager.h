/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_authentication_manager_h
#define cloud_db_authentication_manager_h

#include <random>

#include <utils/common/singleton.h>
#include <utils/network/http/server/abstract_authentication_manager.h>

#include "auth_types.h"


class QnAuthMethodRestrictionList;

namespace nx {
namespace cdb {

class AccountManager;
class SystemManager;
class StreeManager;

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
        const AccountManager& accountManager,
        const SystemManager& systemManager,
        const QnAuthMethodRestrictionList& authRestrictionList,
        const StreeManager& stree);

    //!Implementation of nx_http::AuthenticationManager::authenticate
    virtual bool authenticate(
        const nx_http::HttpServerConnection& connection,
        const nx_http::Request& request,
        boost::optional<nx_http::header::WWWAuthenticate>* const wwwAuthenticate,
        stree::AbstractResourceWriter* authProperties ) override;
        
    static nx::String realm(); 

private:
    const AccountManager& m_accountManager;
    const SystemManager& m_systemManager;
    const QnAuthMethodRestrictionList& m_authRestrictionList;
    const StreeManager& m_stree;
    std::random_device m_rd;
    std::uniform_int_distribution<size_t> m_dist;

    bool findHa1(
        const stree::AbstractResourceReader& authSearchResult,
        const nx_http::StringType& username,
        nx::Buffer* const ha1,
        stree::AbstractResourceWriter* const authProperties ) const;
    void addWWWAuthenticateHeader(
        boost::optional<nx_http::header::WWWAuthenticate>* const wwwAuthenticate );
    nx::Buffer generateNonce();
};

}   //cdb
}   //nx

#endif
