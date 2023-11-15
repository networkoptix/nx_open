// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "authentication_dispatcher.h"

namespace nx::network::http::server {

AuthenticationDispatcher::AuthenticationDispatcher(
    AbstractRequestHandler* defaultHandler)
    :
    base_type(defaultHandler)
{
}

void AuthenticationDispatcher::add(
    const std::regex& pathPattern,
    AbstractRequestHandler* authenticator)
{
    add(pathPattern, nullptr, authenticator);
}

void AuthenticationDispatcher::add(
    const std::regex& pathPattern,
    nx::utils::MoveOnlyFunc<bool(const RequestContext&)> auxiliaryCondition,
    AbstractRequestHandler* authenticator)
{
    auto ctx = std::make_unique<AuthenticatorContext>();
    ctx->pathPattern = pathPattern;
    ctx->cond = std::move(auxiliaryCondition);
    ctx->authenticator = authenticator;

    m_authenticators.push_back(std::move(ctx));
}

void AuthenticationDispatcher::add(
    const std::string& pathPattern,
    AbstractRequestHandler* authenticator)
{
    add(std::regex(pathPattern), authenticator);
}

void AuthenticationDispatcher::serve(
    RequestContext reqCtx,
    nx::utils::MoveOnlyFunc<void(RequestResult)> completionHandler)
{
    AbstractRequestHandler* manager = nullptr;
    std::string path = reqCtx.request.requestLine.url.path().toStdString();

    for (const auto& ctx: m_authenticators)
    {
        if (std::regex_match(path, ctx->pathPattern) &&
            (!ctx->cond || ctx->cond(reqCtx)))
        {
            manager = ctx->authenticator;
            break;
        }
    }

    if (manager)
        manager->serve(std::move(reqCtx), std::move(completionHandler));
    else if (nextHandler())
        nextHandler()->serve(std::move(reqCtx), std::move(completionHandler));
    else
        completionHandler(RequestResult(StatusCode::forbidden));
}

} // namespace nx::network::http::server
