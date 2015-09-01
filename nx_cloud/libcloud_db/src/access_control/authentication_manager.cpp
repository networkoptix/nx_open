/**********************************************************
* 6 may 2015
* a.kolesnikov
***********************************************************/

#include "authentication_manager.h"

#include <limits>

#include <network/auth_restriction_list.h>
#include <utils/network/http/auth_tools.h>

#include "../data/cdb_ns.h"
#include "../managers/account_manager.h"
#include "../managers/system_manager.h"
#include "version.h"


namespace nx {
namespace cdb {

using namespace nx_http;

AuthenticationManager::AuthenticationManager(
    const AccountManager& accountManager,
    const SystemManager& systemManager,
    const QnAuthMethodRestrictionList& authRestrictionList )
:
    m_accountManager( accountManager ),
    m_systemManager( systemManager ),
    m_authRestrictionList( authRestrictionList ),
    m_dist( 0, std::numeric_limits<size_t>::max() )
{
}

bool AuthenticationManager::authenticate(
    const nx_http::HttpServerConnection& connection,
    const nx_http::Request& request,
    boost::optional<nx_http::header::WWWAuthenticate>* const wwwAuthenticate,
    stree::AbstractResourceWriter* authProperties )
{
    //TODO #ak use QnAuthHelper class to support all that authentication types

    const auto allowedAuthMethods = m_authRestrictionList.getAllowedAuthMethods( request );
    if( allowedAuthMethods & AuthMethod::noAuth )
        return true;
    if( !(allowedAuthMethods & AuthMethod::httpDigest) )
        return false;

    auto authHeaderIter = request.headers.find( header::Authorization::NAME );
    if( authHeaderIter == request.headers.end() )
    {
        addWWWAuthenticateHeader( wwwAuthenticate );
        return false;
    }

    //checking header
    header::DigestAuthorization authzHeader;
    if( !authzHeader.parse( authHeaderIter->second ) ||
        authzHeader.authScheme != header::AuthScheme::digest )
    {
        addWWWAuthenticateHeader( wwwAuthenticate );
        return false;
    }

    auto usernameIter = authzHeader.digest->params.find("username");
    if( usernameIter == authzHeader.digest->params.end() )
    {
        addWWWAuthenticateHeader( wwwAuthenticate );
        return false;
    }

    nx::Buffer ha1;
    if( !findHa1(
            connection,
            request,
            usernameIter.value(),
            &ha1,
            authProperties ) )
    {
        addWWWAuthenticateHeader( wwwAuthenticate );
        return false;
    }

    return validateAuthorization(
        request.requestLine.method,
        usernameIter.value(),
        boost::none,
        ha1,
        authzHeader );
}

namespace
{
    const nx::String staticRealm(QN_PRODUCT_NAME_SHORT);
}

nx::String AuthenticationManager::realm() const
{
    return staticRealm;
}

bool AuthenticationManager::findHa1(
    const nx_http::HttpServerConnection& /*connection*/,
    const nx_http::Request& /*request*/,
    const nx_http::StringType& username,
    nx::Buffer* const ha1,
    stree::AbstractResourceWriter* const authProperties ) const
{
    if( auto accountData = m_accountManager.findAccountByUserName( username ) )
    {
        *ha1 = accountData.get().passwordHa1.c_str();
        authProperties->put(
            cdb::param::accountID,
            QVariant::fromValue(accountData.get().id) );
    }
    else if( auto systemData = m_systemManager.findSystemByID( QnUuid::fromStringSafe(username) ) )
    {
        *ha1 = calcHa1(
            username,
            realm(),
            nx::String(systemData.get().authKey.c_str()) );
        authProperties->put(
            cdb::param::systemID,
            QVariant::fromValue(QnUuid(systemData.get().id)) );
    }
    else
    {
        //TODO #ak authenticating other nx_cloud modules by some. 
        //  Probably, should use stree to specify authentication rules
    }

    return true;
}

void AuthenticationManager::addWWWAuthenticateHeader(
    boost::optional<nx_http::header::WWWAuthenticate>* const wwwAuthenticate )
{
    //adding WWW-Authenticate header
    *wwwAuthenticate = header::WWWAuthenticate();
    wwwAuthenticate->get().authScheme = header::AuthScheme::digest;
    wwwAuthenticate->get().params.insert( "nonce", generateNonce() );
    wwwAuthenticate->get().params.insert( "realm", realm() );
    wwwAuthenticate->get().params.insert( "algorithm", "MD5" );
}

nx::Buffer AuthenticationManager::generateNonce()
{
    const size_t nonce = m_dist( m_rd ) | ::time( NULL );
    return nx::Buffer::number( (qulonglong)nonce );
}

}   //cdb
}   //nx
