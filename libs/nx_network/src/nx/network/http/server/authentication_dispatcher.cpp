// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "authentication_dispatcher.h"

namespace nx::network::http::server {

void AuthenticationDispatcher::add(
    const std::regex& pathPattern,
    AbstractAuthenticationManager* authenticator)
{
    m_authenticatorsByRegex.emplace_back(pathPattern, authenticator);
}

void AuthenticationDispatcher::authenticate(
    const nx::network::http::HttpServerConnection& connection,
    const nx::network::http::Request& request,
    AuthenticationCompletionHandler completionHandler)
{
    AbstractAuthenticationManager* manager = nullptr;
    std::string path = request.requestLine.url.path().toStdString();

    {
        for (const auto& element: m_authenticatorsByRegex)
        {
            if (std::regex_match(path, element.first))
            {
                manager = element.second;
                break;
            }
        }
    }

    if (manager)
        manager->authenticate(connection, request, std::move(completionHandler));
    else
        completionHandler(SuccessfulAuthenticationResult());
}

} // namespace nx::network::http::server
