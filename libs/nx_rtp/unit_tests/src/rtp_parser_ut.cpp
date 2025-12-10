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

    virtual nx::rtp::Result processRtpExtension(
        const nx::rtp::RtpHeaderExtensionHeader& extensionHeader,
        quint8* data,
        int size) { rtpExtensionHandler(extensionHeader, data, size); return true; }

    std::function<void (const nx::rtp::RtpHeaderExtensionHeader&, quint8*, int)> rtpExtensionHandler;
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

// See https://www.onvif.org/specs/stream/ONVIF-Streaming-Spec-v1606.pdf
// 6.2.2 Compatibility with the JPEG header extension
TEST(RtpParser, JpegHeaderExtensionfAfterOnvifExtension)
{
    bool packetLost = false;
    bool gotData = false;
    auto codecParser = std::make_unique<MockStreamParser>();
    bool extensionParsed = false;
    codecParser->rtpExtensionHandler =
        [&extensionParsed](const nx::rtp::RtpHeaderExtensionHeader& extensionHeader, quint8* data, int size)
        {
            extensionParsed = true;
            constexpr uint16_t kOnvifJpegExtensionCode = 0xffd8;
            ASSERT_EQ(extensionHeader.id(), kOnvifJpegExtensionCode);
            ASSERT_TRUE(size >= 2);
            ASSERT_EQ(data[0], 0xff);
            ASSERT_EQ(data[1], 0xc0);
        };
    nx::rtp::RtpParser parser(26, std::move(codecParser));
    uint8_t data[] = {
        0x90, 0x1a, 0x64, 0x89, 0x34, 0x25, 0x13, 0x77, 0x1c, 0xd2, 0x5b, 0xeb, 0xab, 0xac, 0x00, 0x0d,
        0xe8, 0x5f, 0xd4, 0xe7, 0x33, 0xc7, 0x4f, 0xb5, 0xa0, 0x05, 0x00, 0x00, 0xff, 0xd8, 0x00, 0x09,
        0xff, 0xc0, 0x00, 0x11, 0x08, 0x07, 0x98, 0x0a, 0x20, 0x03, 0x01, 0x22, 0x00, 0x02, 0x11, 0x01,
        0x03, 0x11, 0x01, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f,
        0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0x00, 0xf3, 0x00, 0x00, 0x00, 0x80,
        0x10, 0x0b, 0x0c, 0x0e, 0x0c, 0x0a, 0x10, 0x0e, 0x0d, 0x0e, 0x12, 0x11, 0x10, 0x13, 0x18, 0x28,
        0x1a, 0x18, 0x16, 0x16, 0x18, 0x31, 0x23, 0x25, 0x1d, 0x28, 0x3a, 0x33, 0x3d, 0x3c, 0x39, 0x33};

    ASSERT_TRUE(parser.processData(data, 0, sizeof(data), packetLost, gotData).success);
    ASSERT_TRUE(extensionParsed);
}


TEST(RtpParser, InvialidOnvifExtension)
{
    bool packetLost = false;
    bool gotData = false;
    auto codecParser = std::make_unique<MockStreamParser>();
    codecParser->rtpExtensionHandler =
        [](const nx::rtp::RtpHeaderExtensionHeader&, quint8*, int)
        {
            // Should not be called. Header is broken.
            ASSERT_TRUE(false);
        };
    nx::rtp::RtpParser parser(96, std::move(codecParser));
    uint8_t data[] = {
        0x90, 0x60, 0xef, 0xeb, 0x00, 0x01, 0x88, 0xda, 0x81, 0x46, 0x30, 0xa5, // RTP headers
        0xab, 0xac, 0x00, 0x02, 0xec, 0xaf, 0x68, 0x9a, 0xd3, 0xbd, 0x16, 0x76, // Broken extension(8 byte)
        0x5c, 0x81, 0xe0, 0x33, 0xe4, 0xac, 0x3b, 0xc1, 0x27, 0x1c, 0x22, 0xde, 0x71, 0x35, 0x22,
        0x75, 0xba, 0x95, 0x00, 0x6c, 0x45, 0xe1, 0x03, 0x92 };

    ASSERT_TRUE(parser.processData(data, 0, sizeof(data), packetLost, gotData).success);
}
