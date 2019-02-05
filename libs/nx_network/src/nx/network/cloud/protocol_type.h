#pragma once

#include <nx/network/abstract_socket.h>

namespace nx::network::cloud {

namespace Protocol
{
    static constexpr int relay = nx::network::Protocol::unassigned + 1;
};

} // namespace nx::network::cloud
