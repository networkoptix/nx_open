#include <gtest/gtest.h>

#include <nx/network/pcp/messaging.h>

#include "data_stream_helpers.h"

namespace nx {
namespace network {
namespace pcp {

static const auto ipHex = QByteArray(16, 'a') + QByteArray(16, 'b');
static const auto ipBin = QByteArray::fromHex(ipHex);

static const auto nonceBin = makeRandomNonce();
static const auto nonceHex = nonceBin.toHex();

TEST(PcpMessaging, RequestHeader)
{
    const RequestHeader h = { VERSION, Opcode::MAP, 0x12345678, ipBin };

    const auto a = dsh::toBytes(h);
    EXPECT_EQ(QByteArray("0201000078563412") + ipHex, a.toHex());

    const auto h2 = dsh::fromBytes<RequestHeader>(a);
    EXPECT_EQ(VERSION, h2.version);
    EXPECT_EQ(Opcode::MAP, h2.opcode);
    EXPECT_EQ((size_t)0x12345678, h2.lifeTime);
    EXPECT_EQ(ipBin, h2.clientIp);
}

TEST(PcpMessaging, ResponseHeadeer)
{
    const ResponseHeadeer h = { VERSION, Opcode::PEER, ResultCode::SUCCESS, 0x12345678, 0xaabbccdd };

    const auto a = dsh::toBytes(h);
    EXPECT_EQ(QByteArray("0282000078563412ddccbbaa000000000000000000000000"), a.toHex());

    const auto h2 = dsh::fromBytes<ResponseHeadeer>(a);
    EXPECT_EQ(VERSION,              h2.version);
    EXPECT_EQ(Opcode::PEER,         h2.opcode);
    EXPECT_EQ(ResultCode::SUCCESS,  h2.resultCode);
    EXPECT_EQ((size_t)0x12345678,           h2.lifeTime);
    EXPECT_EQ((size_t)0xAABBCCDD,           h2.epochTime);
}

TEST(PcpMessaging, MapMessage)
{
    const MapMessage h = { nonceBin, 0x77, 0x1122, 0x3344, ipBin };

    const auto a = dsh::toBytes(h);
    EXPECT_EQ(nonceHex + QByteArray("7700000022114433") + ipHex, a.toHex());

    const auto h2 = dsh::fromBytes<MapMessage>(a);
    EXPECT_EQ(nonceBin, h2.nonce);
    EXPECT_EQ(0x77,     h2.protocol);
    EXPECT_EQ(0x1122,   h2.internalPort);
    EXPECT_EQ(0x3344,   h2.externalPort);
    EXPECT_EQ(ipBin,    h2.externalIp);
}

TEST(PcpMessaging, PeerMessage)
{
    const PeerMessage h = { nonceBin, 0x77, 0x1122, 0x3344, ipBin, 0x5566, ipBin };

    const auto a = dsh::toBytes<PeerMessage>(h);
    EXPECT_EQ(nonceHex + QByteArray("7700000022114433") + ipHex + QByteArray("66550000") + ipHex,
              a.toHex());

    const auto h2 = dsh::fromBytes<PeerMessage>(a);
    EXPECT_EQ(nonceBin, h2.nonce);
    EXPECT_EQ(0x77,     h2.protocol);
    EXPECT_EQ(0x1122,   h2.internalPort);
    EXPECT_EQ(0x3344,   h2.externalPort);
    EXPECT_EQ(ipBin,    h2.externalIp);
    EXPECT_EQ(0x5566,   h2.remotePort);
    EXPECT_EQ(ipBin,    h2.remoteIp);
}

} // namespace pcp
} // namespace network
} // namespace nx
