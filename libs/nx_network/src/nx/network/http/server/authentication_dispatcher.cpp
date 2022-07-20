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
    m_authenticatorsByRegex.emplace_back(pathPattern, authenticator);
}

void AuthenticationDispatcher::add(
    const std::string& pathPattern,
    AbstractRequestHandler* authenticator)
{
    add(std::regex(pathPattern), authenticator);
}

void AuthenticationDispatcher::serve(
    RequestContext ctx,
    nx::utils::MoveOnlyFunc<void(RequestResult)> completionHandler)
{
    AbstractRequestHandler* manager = nullptr;
    std::string path = ctx.request.requestLine.url.path().toStdString();

    for (const auto& element: m_authenticatorsByRegex)
    {
        if (std::regex_match(path, element.first))
        {
            manager = element.second;
            break;
        }
    }

    if (manager)
        manager->serve(std::move(ctx), std::move(completionHandler));
    else if (nextHandler())
        nextHandler()->serve(std::move(ctx), std::move(completionHandler));
    else
        completionHandler(RequestResult(StatusCode::forbidden));
}

} // namespace nx::network::http::server
