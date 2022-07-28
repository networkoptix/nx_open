// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../rest/http_server_rest_message_dispatcher.h"

namespace nx::network::http::server::proxy {

/**
 * Implements reverse proxy.
 * I.e., requests are proxied to specific endpoints depending on the request method & path.
 * Endpoint found in request URL is ignored.
 * Path regexp are checked in decreasing length order.
 */
class NX_NETWORK_API ReverseProxyHandler:
    public AbstractRequestHandler
{
public:
    /**
     * @param target The path found in this URL is ignored. Only scheme and host/port are used.
     * @return true If proxy rule has been created. false if something was already registered on pathRegex.
     */
    bool add(
        const Method& method,
        const std::string_view& pathRegex,
        const nx::utils::Url& target);

    virtual void serve(
        network::http::RequestContext requestContext,
        nx::utils::MoveOnlyFunc<void(network::http::RequestResult)> completionHandler) override;

private:
    rest::MessageDispatcher m_dispatcher;
};

} // namespace nx::network::http::server::proxy
