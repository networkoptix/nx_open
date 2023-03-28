// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/rtp/parsers/rtp_parser.h>

class MockStreamParser: public nx::rtp::StreamParser
{
public:
    virtual void setSdpInfo(const nx::rtp::Sdp::Media&) override {};
    virtual QnAbstractMediaDataPtr nextData() override { return nullptr; };
    virtual nx::rtp::Result processData(
        const nx::rtp::RtpHeader&, quint8*, int, int, bool&) override { return true; }
    virtual void clear() override {};
};

TEST(RtpParser, SimplePacketLoss)
{
    bool packetLost = false;
    bool gotData = false;
    nx::rtp::RtpParser parser(96, std::make_unique<MockStreamParser>());

    uint8_t data[] = {0x80, 0x60, 0x00, /*seq*/ 0x01, 0x00, 0x00, 0x03, 0x84, 0x00, 0x00, 0x02, 0x2b};
    ASSERT_TRUE(parser.processData(data, 0, sizeof(data), packetLost, gotData).success);
    ASSERT_FALSE(packetLost);

    data[3] = 0x02; //< Update sequence number.
    ASSERT_TRUE(parser.processData(data, 0, sizeof(data), packetLost, gotData).success);
    ASSERT_FALSE(packetLost);

    data[3] = 0x04; //< Update sequence number.
    ASSERT_TRUE(parser.processData(data, 0, sizeof(data), packetLost, gotData).success);
    ASSERT_TRUE(packetLost);
}

TEST(RtpParser, NoPacketLossWhenChangeType) // For some buggy cameras
{
    bool packetLost = false;
    bool gotData = false;
    nx::rtp::RtpParser parser(96, std::make_unique<MockStreamParser>());

    uint8_t data[] = {0x80, /*type*/0x60, 0x00, /*seq*/ 0x01, 0x00, 0x00, 0x03, 0x84, 0x00, 0x00, 0x02, 0x2b};
    ASSERT_TRUE(parser.processData(data, 0, sizeof(data), packetLost, gotData).success);
    ASSERT_FALSE(packetLost);

    data[3] = 0x02; //< Update sequence number.
    ASSERT_TRUE(parser.processData(data, 0, sizeof(data), packetLost, gotData).success);
    ASSERT_FALSE(packetLost);

    data[1] = 0x61; //< Update payload type.
    data[3] = 0x04; //< Update sequence number.
    ASSERT_TRUE(parser.processData(data, 0, sizeof(data), packetLost, gotData).success);
    ASSERT_FALSE(packetLost);

    data[1] = 0x60; //< Update payload type.
    data[3] = 0x03; //< Update sequence number.
    ASSERT_TRUE(parser.processData(data, 0, sizeof(data), packetLost, gotData).success);
    ASSERT_FALSE(packetLost);
}
