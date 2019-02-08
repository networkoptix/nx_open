#pragma once

#include "abstract_authentication_manager.h"

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
     */
    void add(
        const std::regex& pathPattern,
        AbstractAuthenticationManager* authenticator);

    virtual void authenticate(
        const nx::network::http::HttpServerConnection& connection,
        const nx::network::http::Request& request,
        AuthenticationCompletionHandler completionHandler) override;

private:
    QnMutex m_mutex;
    std::vector<std::pair<std::regex, AbstractAuthenticationManager*>> m_authenticatorsByRegex;
};

} // namespace nx::network::http::server