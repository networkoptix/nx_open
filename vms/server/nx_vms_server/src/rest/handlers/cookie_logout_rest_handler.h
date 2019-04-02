#pragma once

#include <nx/network/rest/handler.h>

class QnCookieLogoutRestHandler : public nx::network::rest::Handler
{
    Q_OBJECT
public:
    virtual nx::network::rest::Response executeGet(const nx::network::rest::Request& request);
    virtual nx::network::rest::Response executePost(const nx::network::rest::Request& request);

    static void logout(const QnRestConnectionProcessor* connection);
};
