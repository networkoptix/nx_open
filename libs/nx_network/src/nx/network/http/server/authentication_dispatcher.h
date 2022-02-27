// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_authentication_manager.h"

#include <nx/utils/thread/mutex.h>

#include <regex>

namespace nx::network::http::server {

/**
 * Delegates authentication of a HTTP request to AbstractAuthenticationManager*
 * selected by request path.
 * By default, every request is authenticated.
*/
class NX_NETWORK_API AuthenticationDispatcher:
    public AbstractAuthenticationManager
{
public:
    /**
     * Authentication of request is passed to authenticator if
     * Request::requestLine.url.path() is matched against pathPattern.
     * @param authenticator If null, then the request is considered authenticated.
     */
    void add(
        const std::regex& pathPattern,
        AbstractAuthenticationManager* authenticator);

    virtual void authenticate(
        const nx::network::http::HttpServerConnection& connection,
        const nx::network::http::Request& request,
        AuthenticationCompletionHandler completionHandler) override;

private:
    nx::Mutex m_mutex;
    std::vector<std::pair<std::regex, AbstractAuthenticationManager*>> m_authenticatorsByRegex;
};

} // namespace nx::network::http::server
