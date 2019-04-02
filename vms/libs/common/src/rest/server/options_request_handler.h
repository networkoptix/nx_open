#pragma once

#include <nx/network/rest/handler.h>

class OptionsRequestHandler: public nx::network::rest::Handler
{
public:
    virtual nx::network::rest::Response executeRequest(
        const nx::network::rest::Request& request) override;
};
