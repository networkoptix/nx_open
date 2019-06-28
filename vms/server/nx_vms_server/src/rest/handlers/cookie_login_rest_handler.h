#pragma once

#include <nx/network/rest/handler.h>

class QnCookieLoginRestHandler: public nx::network::rest::Handler
{
protected:
    virtual nx::network::rest::Response executePost(
        const nx::network::rest::Request& request) override;
};
