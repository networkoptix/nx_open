// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/stream_server_socket_to_acceptor_wrapper.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/squid_proxy_emulator.h>
#include <nx/network/url/url_parse_helper.h>

#include "tunneling_acceptance_tests.h"

namespace nx::network::http::tunneling::test {

struct MethodMask { static constexpr int value = TunnelMethod::all; };

class HttpTunnelVsFirewall:
    public HttpTunneling<MethodMask>
{
    using base_type = HttpTunneling<MethodMask>;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        givenTunnellingServer();

        m_squid = std::make_unique<network::test::SquidProxyEmulator>();
        m_squid->setDestination(url::getEndpoint(baseUrl()));
        ASSERT_TRUE(m_squid->start());

        setBaseUrl(url::Builder(baseUrl())
            .setEndpoint(m_squid->httpServer().serverAddress()));
    }

private:
    std::unique_ptr<network::test::SquidProxyEmulator> m_squid;
};

TEST_F(HttpTunnelVsFirewall, DISABLED_tunnel_established)
{
    whenRequestTunnel();
    thenTunnelIsEstablished();
}

} // namespace nx::network::http::tunneling::test
