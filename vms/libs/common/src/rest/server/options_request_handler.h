#pragma once

#include <nx/network/rest/handler.h>

class OptionsRequestHandler: public nx::network::rest::Handler
{
private:
    virtual nx::network::rest::Response executeAny(
        const nx::network::rest::Request& request) override;
};
