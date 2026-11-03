// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <cstring>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <nx/media/audio_data_packet.h>
#include <nx/media/video_data_packet.h>
#include <nx/network/test_support/dummy_socket.h>
#include <nx/streaming/rtsp_stream_provider.h>
#include <nx/vms/common/test_support/test_context.h>
#include <rtsp/rtsp_ffmpeg_encoder.h>

namespace nx::streaming::test {

namespace {

// Serves a fixed byte stream, as a server would after the PLAY request.
class ReplaySocket: public nx::network::DummySocket
{
public:
    ReplaySocket(std::string bytes): m_bytes(std::move(bytes)) {}

    bool close() override
    {
        m_isOpened = false;
        return true;
    }
    bool shutdown() override { return close(); }
    bool isClosed() const override { return !m_isOpened; }
    bool isConnected() const override { return m_isOpened; }

    bool connect(const nx::network::SocketAddress&, std::chrono::milliseconds) override
    {
        m_isOpened = true;
        return true;
    }

    int recv(void* buffer, std::size_t bufferLength, int /*flags*/ = 0) override
    {
        const std::size_t size = std::min(bufferLength, m_bytes.size() - m_position);
        std::memcpy(buffer, m_bytes.data() + m_position, size);
        m_position += size;
        return (int) size;
    }

    int send(const void* /*buffer*/, std::size_t bufferLength) override
    {
        return m_isOpened ? (int) bufferLength : -1;
    }

private:
    const std::string m_bytes;
    std::size_t m_position = 0;
    bool m_isOpened = true;
};

// Encodes the packet the way a server sends a track in the Nx proprietary format.
std::string interleavedNxRtp(const QnConstAbstractMediaDataPtr& media, int rtpChannel)
{
    QnRtspFfmpegEncoder encoder(DecoderConfig{}, /*metrics*/ nullptr);
    encoder.setSsrc(rtpChannel * 2);
    encoder.setMtu(1400);
    encoder.setDataPacket(media);

    std::string result;
    nx::utils::ByteArray packet;
    while (encoder.getNextPacket(packet))
    {
        result += '$';
        result += (char) rtpChannel;
        result += (char) (packet.size() >> 8);
        result += (char) (packet.size() & 0xff);
        result.append(packet.data(), packet.size());
        packet.clear();
    }
    return result;
}

} // namespace

class RtspStreamProviderTest: public nx::vms::common::test::ContextBasedTest
{
};

// A server pulling a live stream from another server reads it in play-now mode: no DESCRIBE, so
// tracks are created as their data arrives, and track N arrives on RTP channel 2 * N.
TEST_F(RtspStreamProviderTest, playNowModeKeepsTrackChannels)
{
    auto video = std::make_shared<QnWritableCompressedVideoData>();
    video->compressionType = AV_CODEC_ID_H264;
    video->flags = QnAbstractMediaData::MediaFlags_AVKey;
    video->m_data.write(std::string(100, 'v').data(), 100);

    auto audio = std::make_shared<QnWritableCompressedAudioData>();
    audio->compressionType = AV_CODEC_ID_PCM_MULAW;
    audio->m_data.write(std::string(160, 'a').data(), 160);

    RtspStreamProvider provider(
        systemContext()->globalSettings(), /*timeOffset*/ nullptr, /*forceCameraTime*/ true);
    provider.setUrl(nx::Url("rtsp://127.0.0.1/test"));
    provider.setRtpTransport(nx::vms::api::RtpTransportType::tcp);
    provider.rtspClient().setPlayNowModeAllowed(true);
    provider.rtspClient().setProxySocket(std::make_unique<ReplaySocket>(
        "RTSP/1.0 200 OK\r\nCSeq: 1\r\n\r\n" + interleavedNxRtp(video, /*rtpChannel*/ 0)
        + interleavedNxRtp(video, /*rtpChannel*/ 2) + interleavedNxRtp(audio, /*rtpChannel*/ 4)));
    ASSERT_TRUE(provider.openStream());

    for (const int channel: {0, 1})
    {
        const auto received = provider.getNextData();
        ASSERT_TRUE(received);
        EXPECT_EQ(QnAbstractMediaData::VIDEO, received->dataType);
        EXPECT_EQ(channel, received->channelNumber);
    }

    // On channel 0 the audio would reach the video track encoder, which ends the session.
    const auto received = provider.getNextData();
    ASSERT_TRUE(received);
    EXPECT_EQ(QnAbstractMediaData::AUDIO, received->dataType);
    EXPECT_EQ(2, received->channelNumber);
}

} // namespace nx::streaming::test
