// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "request_decorator_handler.h"

namespace nx::network::http::server {

void RequestDecoratorHandler::serve(
    RequestContext requestContext,
    nx::utils::MoveOnlyFunc<void(RequestResult)> handler)
{
    for (const auto& [name, value]: m_attributes)
        requestContext.attrs[name] = value;

    nextHandler()->serve(std::move(requestContext), std::move(handler));
}

void RequestDecoratorHandler::addAttributes(
    const std::vector<std::pair<std::string, std::string>>& attributes)
{
    m_attributes = attributes;
}

} // namespace nx::network::http::server
