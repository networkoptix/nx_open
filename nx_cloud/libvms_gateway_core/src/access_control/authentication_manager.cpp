/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#include "authentication_manager.h"

#include <future>
#include <limits>

#include <boost/optional.hpp>

#include <nx/network/app_info.h>
#include <nx/network/http/auth_restriction_list.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/server/fusion_request_result.h>
#include <nx/network/http/server/http_server_connection.h>

#include <nx/network/http/custom_headers.h>
#include <nx/utils/scope_guard.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/lexical.h>

#include "stree/cdb_ns.h"
#include "stree/http_request_attr_reader.h"
#include "stree/socket_attr_reader.h"


namespace nx {
namespace cloud {
namespace gateway {

using namespace nx_http;

AuthenticationManager::AuthenticationManager(
    const nx::network::http::AuthMethodRestrictionList& authRestrictionList,
    const nx::utils::stree::StreeManager& stree)
:
    m_authRestrictionList(authRestrictionList),
    m_stree(stree),
    m_dist(0, std::numeric_limits<size_t>::max())
{
}

void AuthenticationManager::authenticate(
    const nx::network::http::HttpServerConnection& connection,
    const nx::network::http::Request& request,
    nx::network::http::server::AuthenticationCompletionHandler completionHandler)
{
    nx::network::http::server::AuthenticationResult authenticationResult;
    auto scopedGuard = makeScopeGuard(
        [&authenticationResult, &completionHandler]() mutable
        {
            completionHandler(std::move(authenticationResult));
        });

    const auto allowedAuthMethods = m_authRestrictionList.getAllowedAuthMethods(request);
    if (allowedAuthMethods & AuthMethod::noAuth)
    {
        authenticationResult.isSucceeded = true;
        return;
    }
    if (!(allowedAuthMethods & AuthMethod::httpDigest))
    {
        authenticationResult.isSucceeded = false;
        return;
    }

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
    nx::utils::stree::ResourceContainer authTraversalResult;
    nx::utils::stree::ResourceContainer inputRes;
    if (authzHeader && !authzHeader->userid().isEmpty())
        inputRes.put(attr::userName, authzHeader->userid());
    SocketResourceReader socketResources(*connection.socket());
    HttpRequestResourceReader httpRequestResources(request);
    const auto authSearchInputData = nx::utils::stree::MultiSourceResourceReader(
        socketResources,
        httpRequestResources,
        inputRes);
    m_stree.search(
        authSearchInputData,
        &authTraversalResult);
    if (auto authenticated = authTraversalResult.get(attr::authenticated))
    {
        if (authenticated.get().toBool())
        {
            authenticationResult.isSucceeded = true;
            return;
        }
    }

    if (!authzHeader ||
        (authzHeader->authScheme != header::AuthScheme::digest) ||
        (authzHeader->userid().isEmpty()))
    {
        addWWWAuthenticateHeader(&authenticationResult.wwwAuthenticate);
        authenticationResult.isSucceeded = false;
        return;
    }

    if (!validateNonce(authzHeader->digest->params["nonce"]))
    {
        addWWWAuthenticateHeader(&authenticationResult.wwwAuthenticate);
        authenticationResult.isSucceeded = false;
        return;
    }

    const auto userID = authzHeader->userid();
    std::function<bool(const nx::Buffer&)> validateHa1Func = std::bind(
        &validateAuthorization,
        request.requestLine.method,
        userID,
        boost::none,
        std::placeholders::_1,
        std::move(authzHeader.get()));

    //analyzing authSearchResult for password or ha1 pesence
    if (auto foundHa1 = authTraversalResult.get(attr::ha1))
    {
        if (validateHa1Func(foundHa1.get().toString().toLatin1()))
        {
            authenticationResult.isSucceeded = true;
            return;
        }
    }
    if (auto password = authTraversalResult.get(attr::userPassword))
    {
        if (validateHa1Func(calcHa1(
                userID,
                realm(),
                password.get().toString())))
        {
            authenticationResult.isSucceeded = true;
            return;
        }
    }

    authenticationResult.isSucceeded = false;
}

nx::String AuthenticationManager::realm()
{
    return nx::network::AppInfo::realm().toUtf8();
}

bool AuthenticationManager::validateNonce(const nx::network::http::StringType& nonce)
{
    //TODO #ak introduce more strong nonce validation
        //Currently, forbidding authentication with nonce, generated by /auth/get_nonce request
    return nonce.size() < 31;
}

void AuthenticationManager::addWWWAuthenticateHeader(
    boost::optional<nx::network::http::header::WWWAuthenticate>* const wwwAuthenticate)
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

}   //namespace cloud
}   //namespace gateway
}   //namespace nx
