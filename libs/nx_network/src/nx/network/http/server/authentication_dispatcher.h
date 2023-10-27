// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <regex>
#include <vector>

#include "abstract_authentication_manager.h"

namespace nx::network::http::server {

/**
 * Delegates authentication of a HTTP request to AbstractAuthenticationManager*
 * selected by request path.
 * If a defaultHandler was supplied to the constructor, then it receives requests not matched for
 * any authentication manager.
 * If no defaultHandler present, then not matched requests are completed with 403 Forbidden status code.
 * Note: the class methods are not thread-safe. So, the class must be configured before first usage.
 *
 * TODO: #akolesnikov remove this class and replaces usages with rest::MessageDispatcher.
 */
class NX_NETWORK_API AuthenticationDispatcher:
    public AbstractAuthenticationManager
{
    using base_type = AbstractAuthenticationManager;

public:
    /**
     * @param defaultHandler If not null, it receives requests that were not matched for any
     * authentication manager.
     */
    AuthenticationDispatcher(AbstractRequestHandler* defaultHandler);

    /**
     * Authentication of request is passed to authenticator if Request::requestLine.url.path()
     * is matched against pathPattern.
     */
    void add(
        const std::regex& pathPattern,
        AbstractRequestHandler* authenticator);

    /**
     * Register authenticator that is chosen not only by path, but also by auxiliaryCondition.
     */
    void add(
        const std::regex& pathPattern,
        nx::utils::MoveOnlyFunc<bool(const RequestContext&)> auxiliaryCondition,
        AbstractRequestHandler* authenticator);

    void add(
        const std::string& pathPattern,
        AbstractRequestHandler* authenticator);

    virtual void serve(
        RequestContext requestContext,
        nx::utils::MoveOnlyFunc<void(RequestResult)> completionHandler) override;

private:
    struct AuthenticatorContext
    {
        std::regex pathPattern;
        nx::utils::MoveOnlyFunc<bool(const RequestContext&)> cond = nullptr;
        AbstractRequestHandler* authenticator = nullptr;
    };

private:
    std::vector<std::shared_ptr<AuthenticatorContext>> m_authenticators;
};

} // namespace nx::network::http::server
