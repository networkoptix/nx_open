/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#include "authentication_manager.h"

#include <future>
#include <limits>

#include <boost/optional.hpp>

#include <nx/network/auth_restriction_list.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/server/fusion_request_result.h>

#include <http/custom_headers.h>
#include <utils/common/app_info.h>
#include <utils/common/guard.h>
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
    const QnAuthMethodRestrictionList& authRestrictionList,
    const stree::StreeManager& stree)
:
    m_authRestrictionList(authRestrictionList),
    m_stree(stree),
    m_dist(0, std::numeric_limits<size_t>::max())
{
}

void AuthenticationManager::authenticate(
    const nx_http::HttpServerConnection& connection,
    const nx_http::Request& request,
    nx_http::AuthenticationCompletionHandler completionHandler)
{
    bool authenticationResult = false;
    stree::ResourceContainer authInfo;
    boost::optional<nx_http::header::WWWAuthenticate> wwwAuthenticate;
    nx_http::HttpHeaders responseHeaders;
    std::unique_ptr<nx_http::AbstractMsgBodySource> msgBody;
    auto scopedGuard = makeScopedGuard(
        [&authenticationResult, &authInfo, &wwwAuthenticate,
            &responseHeaders, &msgBody, &completionHandler]() mutable
        {
            completionHandler(
                authenticationResult,
                std::move(authInfo),
                std::move(wwwAuthenticate),
                std::move(responseHeaders),
                std::move(msgBody));
        });

    const auto allowedAuthMethods = m_authRestrictionList.getAllowedAuthMethods(request);
    if (allowedAuthMethods & AuthMethod::noAuth)
    {
        authenticationResult = true;
        return;
    }
    if (!(allowedAuthMethods & AuthMethod::httpDigest))
    {
        authenticationResult = false;
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
    stree::ResourceContainer authTraversalResult;
    stree::ResourceContainer inputRes;
    if (authzHeader && !authzHeader->userid().isEmpty())
        inputRes.put(attr::userName, authzHeader->userid());
    SocketResourceReader socketResources(*connection.socket());
    HttpRequestResourceReader httpRequestResources(request);
    const auto authSearchInputData = stree::MultiSourceResourceReader(
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
            authenticationResult = true;
            return;
        }
    }

    if (!authzHeader ||
        (authzHeader->authScheme != header::AuthScheme::digest) ||
        (authzHeader->userid().isEmpty()))
    {
        addWWWAuthenticateHeader(&wwwAuthenticate);
        authenticationResult = false;
        return;
    }

    if (!validateNonce(authzHeader->digest->params["nonce"]))
    {
        addWWWAuthenticateHeader(&wwwAuthenticate);
        authenticationResult = false;
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
            authenticationResult = true;
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
            authenticationResult = true;
            return;
        }
    }

    authenticationResult = false;
}

nx::String AuthenticationManager::realm()
{
    return QnAppInfo::realm().toUtf8();
}

bool AuthenticationManager::validateNonce(const nx_http::StringType& nonce)
{
    //TODO #ak introduce more strong nonce validation
        //Currently, forbidding authentication with nonce, generated by /auth/get_nonce request
    return nonce.size() < 31;
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

}   //namespace cloud
}   //namespace gateway
}   //namespace nx
