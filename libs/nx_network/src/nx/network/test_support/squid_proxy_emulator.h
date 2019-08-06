#pragma once

#include <nx/network/stream_proxy.h>

namespace nx::network::test {

class NX_NETWORK_API SquidProxyEmulator:
    public StreamProxy
{
public:
    SquidProxyEmulator();
};

} // namespace nx::network::test
