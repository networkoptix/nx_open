#include <gtest/gtest.h>
#include <utils/network/pcp/messaging.h>

#include <QDebug>

using namespace pcp;

TEST(PcpMessaging, RequestHeader)
{
    const RequestHeader h = { VERSION, Opcode::MAP, 0x12345678, 0xAABBCCDD };

    const auto a = toBytes(h);
    EXPECT_EQ(QByteArray("0201000012345678AABBCCDD"), a.toHex().toUpper());

    const auto h2 = fromBytes<RequestHeader>(a);
    EXPECT_EQ(VERSION,       h2.version);
    EXPECT_EQ(Opcode::MAP,   h2.opcode);
    EXPECT_EQ(0x12345678,    h2.lifeTime);
    EXPECT_EQ(0xAABBCCDD,    h2.clientIp);
}

TEST(PcpMessaging, ResponseHeadeer)
{
    const ResponseHeadeer h = { VERSION, Opcode::PEER, ResultCode::SUCCESS, 0x12345678, 0xAABBCCDD };

    const auto a = toBytes(h);
    const QByteArray msg("0282000012345678AABBCCDD000000000000000000000000");
    EXPECT_EQ(msg, a.toHex().toUpper());

    const auto h2 = fromBytes<ResponseHeadeer>(a);
    EXPECT_EQ(VERSION,              h2.version);
    EXPECT_EQ(Opcode::PEER,         h2.opcode);
    EXPECT_EQ(ResultCode::SUCCESS,  h2.resultCode);
    EXPECT_EQ(0x12345678,           h2.lifeTime);
    EXPECT_EQ(0xAABBCCDD,           h2.epochTime);
}

TEST(PcpMessaging, MapMessage)
{
    const auto nonce = makeRandomNonce();
    const MapMessage h = { nonce, 0x77, 0x1111, 0x2222, 0xAABBCCDD };

    const auto a = toBytes(h);
    QByteArray msg("7700000011112222AABBCCDD");
    msg.push_front(nonce.toHex().toUpper());
    EXPECT_EQ(msg, a.toHex().toUpper());

    const auto h2 = fromBytes<MapMessage>(a);
    EXPECT_EQ(nonce,            h2.nonce);
    EXPECT_EQ(0x77,             h2.protocol);
    EXPECT_EQ(0x1111,           h2.internalPort);
    EXPECT_EQ(0x2222,           h2.externalPort);
    EXPECT_EQ(0xAABBCCDD,       h2.externalIp);
}

TEST(PcpMessaging, PeerMessage)
{
    const auto nonce = makeRandomNonce();
    const PeerMessage h = { nonce, 0x77, 0x1111, 0x2222, 0xAABBCCDD, 0x3333, 0xDDCCBBAA };

    const auto a = toBytes<PeerMessage>(h);
    QByteArray msg("7700000011112222AABBCCDD33330000DDCCBBAA");
    msg.push_front(nonce.toHex().toUpper());
    EXPECT_EQ(msg, a.toHex().toUpper());

    const auto h2 = fromBytes<PeerMessage>(a);
    EXPECT_EQ(nonce,            h2.nonce);
    EXPECT_EQ(0x77,             h2.protocol);
    EXPECT_EQ(0x1111,           h2.internalPort);
    EXPECT_EQ(0x2222,           h2.externalPort);
    EXPECT_EQ(0xAABBCCDD,       h2.externalIp);
    EXPECT_EQ(0x3333,           h2.remotePort);
    EXPECT_EQ(0xDDCCBBAA,       h2.remoteIp);
}
