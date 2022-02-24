// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/test_http_server.h>
#include <nx/network/stream_proxy.h>

namespace nx::network::test {

class NX_NETWORK_API SquidProxyEmulator
{
public:
    bool start();

    http::TestHttpServer& httpServer() { return m_httpServer; }

    void setDestination(const SocketAddress&);

private:
    SocketAddress m_targetAddress;
    http::TestHttpServer m_httpServer;
};

//-------------------------------------------------------------------------------------------------

namespace deprecated {

class NX_NETWORK_API SquidProxyEmulator:
    public StreamProxy
{
public:
    SquidProxyEmulator();
};

} // namespace deprecated

} // namespace nx::network::test
