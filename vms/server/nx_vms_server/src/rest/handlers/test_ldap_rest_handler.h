#pragma once

#include <nx/network/rest/handler.h>

namespace nx::vms::server::rest {

class TestLdapSettingsHandler: public nx::network::rest::Handler
{
protected:
    nx::network::rest::Response executePost(const nx::network::rest::Request& request) override;
};

} // namespace nx::vms::server::rest
