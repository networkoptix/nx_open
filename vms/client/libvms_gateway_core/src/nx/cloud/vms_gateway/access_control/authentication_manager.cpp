// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "authentication_manager.h"

#include <future>
#include <limits>
#include <optional>

#include <nx/network/app_info.h>
#include <nx/network/http/auth_restriction_list.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/server/api_request_result.h>
#include <nx/network/http/server/http_server_connection.h>

#include <nx/network/http/custom_headers.h>
#include <nx/utils/scope_guard.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/lexical.h>

#include "../stree/cdb_ns.h"
#include "../stree/http_request_attr_reader.h"
#include "../stree/socket_attr_reader.h"


namespace nx {
namespace cloud {
namespace gateway {

using namespace nx::network::http;

AuthenticationManager::AuthenticationManager(
    nx::network::http::AbstractRequestHandler* requestHandler,
    const nx::network::http::AuthMethodRestrictionList& authRestrictionList,
    const nx::utils::stree::StreeManager& stree)
:
    base_type(requestHandler),
    m_authRestrictionList(authRestrictionList),
    m_stree(stree),
    m_dist(0, std::numeric_limits<size_t>::max())
{
}

void AuthenticationManager::serve(
    nx::network::http::RequestContext ctx,
    nx::utils::MoveOnlyFunc<void(nx::network::http::RequestResult)> completionHandler)
{
    nx::network::http::RequestResult authenticationResult(
        nx::network::http::StatusCode::unauthorized);
    auto scopedGuard = nx::utils::makeScopeGuard(
        [this, &ctx, &authenticationResult, &completionHandler]() mutable
        {
            if (authenticationResult.statusCode == nx::network::http::StatusCode::ok)
            {
                nextHandler()->serve(std::move(ctx), std::move(completionHandler));
                return;
            }

            completionHandler(std::move(authenticationResult));
        });

    const auto allowedAuthMethods = m_authRestrictionList.getAllowedAuthMethods(ctx.request);
    if (allowedAuthMethods & AuthMethod::noAuth)
    {
        authenticationResult.statusCode = nx::network::http::StatusCode::ok;
        return;
    }
    if (!(allowedAuthMethods & AuthMethod::httpDigest))
    {
        authenticationResult.statusCode = nx::network::http::StatusCode::unauthorized;
        return;
    }

    const auto authHeaderIter = ctx.request.headers.find(header::Authorization::NAME);

    //checking header
    std::optional<header::DigestAuthorization> authHeader;
    if (authHeaderIter != ctx.request.headers.end())
    {
        authHeader.emplace(header::DigestAuthorization());
        if (!authHeader->parse(authHeaderIter->second))
            authHeader.reset();
    }

    //performing stree search
    nx::utils::stree::AttributeDictionary authTraversalResult;
    nx::utils::stree::AttributeDictionary inputRes;
    if (authHeader && !authHeader->userid().empty())
        inputRes.putStr(attr::userName, authHeader->userid());
    SocketResourceReader socketResources(ctx.connectionAttrs);
    HttpRequestResourceReader httpRequestResources(ctx.request);
    const auto authSearchInputData = nx::utils::stree::makeMultiReader(
        socketResources,
        httpRequestResources,
        inputRes);
    m_stree.search(authSearchInputData, &authTraversalResult);
    if (auto authenticated = authTraversalResult.get<bool>(attr::authenticated))
    {
        if (*authenticated)
        {
            authenticationResult.statusCode = nx::network::http::StatusCode::ok;
            return;
        }
    }

    if (!authHeader ||
        (authHeader->authScheme != header::AuthScheme::digest) ||
        (authHeader->userid().empty()) ||
        !validateNonce(authHeader->digest->params["nonce"]))
    {
        authenticationResult.headers.emplace(
            nx::network::http::header::WWWAuthenticate::NAME,
            prepareWWWAuthenticateHeader().serialized());
        authenticationResult.statusCode = nx::network::http::StatusCode::unauthorized;
        return;
    }

    const auto userID = authHeader->userid();

    auto validateHa1Func =
        [&ctx, &userID, &authHeader](const nx::Buffer& buffer) -> bool
        {
            return validateAuthorization(ctx.request.requestLine.method, userID, {}, buffer,
                std::move(*authHeader));
        };

    //analyzing authSearchResult for password or ha1 pesence
    if (auto foundHa1 = authTraversalResult.get<std::string>(attr::ha1))
    {
        if (validateHa1Func(*foundHa1))
        {
            authenticationResult.statusCode = nx::network::http::StatusCode::ok;
            return;
        }
    }
    if (auto password = authTraversalResult.get<std::string>(attr::userPassword))
    {
        if (validateHa1Func(calcHa1(userID, realm(), *password)))
        {
            authenticationResult.statusCode = nx::network::http::StatusCode::ok;
            return;
        }
    }

    authenticationResult.statusCode = nx::network::http::StatusCode::unauthorized;
}

std::string AuthenticationManager::realm()
{
    return nx::network::AppInfo::realm();
}

bool AuthenticationManager::validateNonce(const std::string& nonce)
{
    //TODO #akolesnikov introduce more strong nonce validation
        //Currently, forbidding authentication with nonce, generated by /auth/get_nonce request
    return nonce.size() < 31;
}

nx::network::http::header::WWWAuthenticate AuthenticationManager::prepareWWWAuthenticateHeader()
{
    //adding WWW-Authenticate header
    header::WWWAuthenticate wwwAuthenticate;
    wwwAuthenticate.authScheme = header::AuthScheme::digest;
    wwwAuthenticate.params.emplace("nonce", generateNonce());
    wwwAuthenticate.params.emplace("realm", realm());
    wwwAuthenticate.params.emplace("algorithm", "MD5");
    return wwwAuthenticate;
}

nx::Buffer AuthenticationManager::generateNonce()
{
    const size_t nonce = m_dist(m_rd) | ::time(nullptr);
    return std::to_string(nonce);
}

}   //namespace cloud
}   //namespace gateway
}   //namespace nx
