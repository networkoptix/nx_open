#pragma once

#include <nx/network/rest/handler.h>

class QnLogLevelRestHandler: public nx::network::rest::Handler
{
public:
    nx::network::rest::Response executeGet(const nx::network::rest::Request& request) override;

private:
    virtual nx::network::rest::Response manageLogLevelById(
        const nx::network::rest::Request& request);
};
