#pragma once

#include <nx/network/rest/handler.h>

namespace nx::vms::server {

class HardwareInfoHandler: public nx::network::rest::Handler
{
protected:
    nx::network::rest::Response executeGet(const nx::network::rest::Request& request) override;
};

} // namespace nx::vms::server
