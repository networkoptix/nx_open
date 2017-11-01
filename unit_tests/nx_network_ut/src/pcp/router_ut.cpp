#include <gtest/gtest.h>

#include <nx/network/pcp/router_pcp.h>

#include "data_stream_helpers.h"

class TestRouter : protected pcp::Router
{
public:
    using Router::makeMapRequest;
    using Router::parseMapResponse;
};

TEST(PcpRouter, StaticMap)
{
    const HostAddress localhost("192.168.0.10");
    pcp::Router::Mapping mapping;
    mapping.internal = SocketAddress(localhost, 0x1234);

    const auto request = TestRouter::makeMapRequest(mapping);
    const auto header = QByteArray("02010000100e0000") + dsh::rawBytes(*localhost.ipV6().first).toHex();
    const auto nonce = mapping.nonce.toHex();
    const auto nullv6 = QByteArray(16, 0).toHex();
    EXPECT_EQ(header + nonce + QByteArray("0000000034123412") + nullv6, request.toHex());

    const HostAddress external("32.43.12.43");
    const auto header2 = QByteArray("02810000100e0000") + dsh::rawBytes(*localhost.ipV6().first).toHex();
    const auto response = header2 + nonce + QByteArray("0000000034123412") +
            dsh::rawBytes(*external.ipV6().first).toHex();

    EXPECT_TRUE(TestRouter::parseMapResponse(QByteArray::fromHex(response), mapping));
    EXPECT_EQ(mapping.external.toString().toUtf8(), QByteArray("32.43.12.43:4660"));
    EXPECT_GT(mapping.lifeTime, (size_t)0);
}

TEST(PcpRouter, DISABLED_RealMap)
{
    pcp::Router::Mapping mapping;
    mapping.internal = SocketAddress("10.0.2.134", 80);
    mapping.external.port = 8877;

    pcp::Router router("10.0.2.103");
    router.mapPort(mapping);

    EXPECT_GT(mapping.lifeTime, 0U);
}
