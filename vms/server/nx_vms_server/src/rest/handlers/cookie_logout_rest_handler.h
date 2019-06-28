#pragma once

#include <nx/network/rest/handler.h>

class QnCookieLogoutRestHandler : public nx::network::rest::Handler
{
protected:
    virtual nx::network::rest::Response executeGet(const nx::network::rest::Request& request);
    virtual nx::network::rest::Response executePost(const nx::network::rest::Request& request);

public:
    static void logout(const QnRestConnectionProcessor* connection);
};
