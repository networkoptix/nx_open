// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <memory>

#include <gtest/gtest.h>

#include <nx/media/meta_data_packet.h>
#include <nx/streaming/rtp/parsers/nx_rtp_parser.h>
#include <rtsp/rtsp_ffmpeg_encoder.h>
#include <utils/common/synctime.h>

using namespace std::chrono;

namespace nx::rtp::test {

namespace {

// A motion packet carrying the given number of grid bytes instead of the whole grid.
QnMetaDataV1Ptr makeMotionPacket(int gridSize)
{
    auto packet = std::make_shared<QnMetaDataV1>(microseconds(1000000));
    packet->m_data.clear();
    packet->m_data.writeFiller(0, gridSize);
    return packet;
}

QnCompressedMetadataPtr makeObjectsPacket(int payloadSize)
{
    auto packet = std::make_shared<QnCompressedMetadata>(MetadataType::ObjectDetection);
    packet->timestamp = microseconds(1000000).count();
    packet->m_data.writeFiller(0, payloadSize);
    return packet;
}

} // namespace

class NxRtpParserTest: public ::testing::Test
{
protected:
    // Feeds the packet to the parser the way a Server sends it: as Nx proprietary RTP.
    Result send(const QnConstAbstractMediaDataPtr& media, bool* gotData)
    {
        QnRtspFfmpegEncoder encoder(DecoderConfig(), /*metrics*/ nullptr);
        encoder.setSsrc(0); //< An even SSRC marks a data track, an odd one codec parameters.
        encoder.setMtu(1400);
        encoder.setDataPacket(media);

        *gotData = false;
        Result result(true);
        nx::utils::ByteArray rtpPacket;
        while (result.success && encoder.getNextPacket(rtpPacket))
        {
            result = m_parser.processData(
                (const quint8*) rtpPacket.data(), (int) rtpPacket.size(), *gotData);
            rtpPacket.clear();
        }
        return result;
    }

    QnSyncTime m_syncTime; //< The parser timestamps the packets it builds with it.
    QnNxRtpParser m_parser;
};

TEST_F(NxRtpParserTest, motionPacketIsParsed)
{
    bool gotData = false;
    const Result result = send(makeMotionPacket(QnMetaDataV1::kMotionDataBufferSize), &gotData);
    ASSERT_TRUE(result.success) << result.message.toStdString();
    ASSERT_TRUE(gotData);

    const auto parsed = std::dynamic_pointer_cast<QnMetaDataV1>(m_parser.nextData());
    ASSERT_TRUE(parsed);
    ASSERT_EQ((size_t) QnMetaDataV1::kMotionDataBufferSize, parsed->dataSize());
}

// A motion packet is a fixed size grid and the whole QnMetaDataV1 API relies on that size, so a
// truncated one must be rejected instead of being passed to the motion consumers, which would
// read and write past the end of the buffer.
TEST_F(NxRtpParserTest, truncatedMotionPacketIsRejected)
{
    bool gotData = false;
    const Result result = send(makeMotionPacket(10), &gotData);
    ASSERT_FALSE(result.success);
    ASSERT_FALSE(gotData);
    ASSERT_FALSE(m_parser.nextData());
}

// Only motion has a fixed size: other metadata keeps arriving with an arbitrary payload, split
// between as many RTP packets as it takes.
TEST_F(NxRtpParserTest, objectMetadataOfAnySizeIsParsed)
{
    for (const int payloadSize: {10, 5000})
    {
        SCOPED_TRACE(payloadSize);

        bool gotData = false;
        const Result result = send(makeObjectsPacket(payloadSize), &gotData);
        ASSERT_TRUE(result.success) << result.message.toStdString();
        ASSERT_TRUE(gotData);

        const auto parsed =
            std::dynamic_pointer_cast<QnAbstractCompressedMetadata>(m_parser.nextData());
        ASSERT_TRUE(parsed);
        EXPECT_EQ(MetadataType::ObjectDetection, parsed->metadataType);
        EXPECT_EQ((size_t) payloadSize, parsed->dataSize());
    }
}

} // namespace nx::rtp::test
