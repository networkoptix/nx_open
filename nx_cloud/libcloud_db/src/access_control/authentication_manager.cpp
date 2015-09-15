/**********************************************************
* 6 may 2015
* a.kolesnikov
***********************************************************/

#include "authentication_manager.h"

#include <limits>

#include <boost/optional.hpp>

#include <network/auth_restriction_list.h>
#include <utils/network/http/auth_tools.h>

#include "stree/cdb_ns.h"
#include "managers/account_manager.h"
#include "managers/system_manager.h"
#include "stree/http_request_attr_reader.h"
#include "stree/socket_attr_reader.h"
#include "stree/stree_manager.h"
#include "version.h"


namespace nx {
namespace cdb {

using namespace nx_http;

AuthenticationManager::AuthenticationManager(
    const AccountManager& accountManager,
    const SystemManager& systemManager,
    const QnAuthMethodRestrictionList& authRestrictionList,
    const StreeManager& stree)
:
    m_accountManager(accountManager),
    m_systemManager(systemManager),
    m_authRestrictionList(authRestrictionList),
    m_stree(stree),
    m_dist(0, std::numeric_limits<size_t>::max())
{
}

bool AuthenticationManager::authenticate(
    const nx_http::HttpServerConnection& connection,
    const nx_http::Request& request,
    boost::optional<nx_http::header::WWWAuthenticate>* const wwwAuthenticate,
    stree::AbstractResourceWriter* authProperties)
{
    //TODO #ak use QnAuthHelper class to support all that authentication types

    const auto allowedAuthMethods = m_authRestrictionList.getAllowedAuthMethods(request);
    if (allowedAuthMethods & AuthMethod::noAuth)
        return true;
    if (!(allowedAuthMethods & AuthMethod::httpDigest))
        return false;

    const auto authHeaderIter = request.headers.find(header::Authorization::NAME);

    //checking header
    boost::optional<header::DigestAuthorization> authzHeader;
    if (authHeaderIter != request.headers.end())
    {
        authzHeader.emplace(header::DigestAuthorization());
        if (!authzHeader->parse(authHeaderIter->second))
            authzHeader.reset();
    }

    //performing stree search
    stree::ResourceContainer authResult;
    stree::ResourceContainer inputRes;
    if (authzHeader && !authzHeader->userid().isEmpty())
        inputRes.put(attr::userName, authzHeader->userid());
    SocketResourceReader socketResources(*connection.socket());
    HttpRequestResourceReader httpRequestResources(request);
    m_stree.search(
        StreeOperation::authentication,
        stree::MultiSourceResourceReader(socketResources, httpRequestResources, inputRes),
        &authResult);
    if (auto authenticated = authResult.get(attr::authenticated))
    {
        if (authenticated.get().toBool())
            return true;
    }

    if (!authzHeader ||
        (authzHeader->authScheme != header::AuthScheme::digest) ||
        (authzHeader->userid().isEmpty()))
    {
        addWWWAuthenticateHeader(wwwAuthenticate);
        return false;
    }

    nx::Buffer ha1;
    if (!findHa1(
        authResult,
        authzHeader->userid(),
        &ha1,
        authProperties))
    {
        addWWWAuthenticateHeader(wwwAuthenticate);
        return false;
    }

    return validateAuthorization(
        request.requestLine.method,
        authzHeader->userid(),
        boost::none,
        ha1,
        authzHeader.get());
}

namespace
{
    const nx::String staticRealm(QN_REALM);
}

nx::String AuthenticationManager::realm()
{
    return staticRealm;
}

bool AuthenticationManager::findHa1(
    const stree::AbstractResourceReader& authSearchResult,
    const nx_http::StringType& username,
    nx::Buffer* const ha1,
    stree::AbstractResourceWriter* const authProperties) const
{
    //TODO #ak analyzing authSearchResult for password or ha1 pesence
    if (auto foundHa1 = authSearchResult.get(attr::ha1))
    {
        *ha1 = foundHa1.get().toString().toLatin1();
        return true;
    }
    if (auto password = authSearchResult.get(attr::userPassword))
    {
        *ha1 = calcHa1(
            username,
            realm(),
            password.get().toString());
        return true;
    }

    if (auto accountData = m_accountManager.findAccountByUserName(username))
    {
        *ha1 = accountData.get().passwordHa1.c_str();
        authProperties->put(
            cdb::attr::accountID,
            QVariant::fromValue(accountData.get().id));
    }
    else if (auto systemData = m_systemManager.findSystemByID(QnUuid::fromStringSafe(username)))
    {
        //TODO #ak successfull system authentication should activate system
        *ha1 = calcHa1(
            username,
            realm(),
            nx::String(systemData.get().authKey.c_str()));
        authProperties->put(
            cdb::attr::systemID,
            QVariant::fromValue(QnUuid(systemData.get().id)));
    }

    return true;
}

void AuthenticationManager::addWWWAuthenticateHeader(
    boost::optional<nx_http::header::WWWAuthenticate>* const wwwAuthenticate)
{
    //adding WWW-Authenticate header
    *wwwAuthenticate = header::WWWAuthenticate();
    wwwAuthenticate->get().authScheme = header::AuthScheme::digest;
    wwwAuthenticate->get().params.insert("nonce", generateNonce());
    wwwAuthenticate->get().params.insert("realm", realm());
    wwwAuthenticate->get().params.insert("algorithm", "MD5");
}

nx::Buffer AuthenticationManager::generateNonce()
{
    const size_t nonce = m_dist(m_rd) | ::time(NULL);
    return nx::Buffer::number((qulonglong)nonce);
}

}   //cdb
}   //nx
