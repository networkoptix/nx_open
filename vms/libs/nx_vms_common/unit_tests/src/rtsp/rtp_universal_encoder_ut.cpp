// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/media/audio_data_packet.h>
#include <nx/rtp/rtp.h>
#include <rtsp/rtp_universal_encoder.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace {

CodecParametersPtr opusCodecParameters(int channels)
{
    auto parameters = std::make_shared<CodecParameters>();
    auto* av = parameters->getAvCodecParameters();
    av->codec_type = AVMEDIA_TYPE_AUDIO;
    av->codec_id = AV_CODEC_ID_OPUS;
    av->sample_rate = 48000;
    av_channel_layout_default(&av->ch_layout, channels);
    return parameters;
}

// As nx::webrtc::Transcoder::createRtpEncoder() configures it.
QnUniversalRtpEncoder::Config webRtcConfig()
{
    QnUniversalRtpEncoder::Config config;
    config.absoluteRtcpTimestamps = true;
    config.useRtcpNack = true;
    return config;
}

int payloadTypeFromSdpMediaLine(const QString& sdp)
{
    for (const auto& line: sdp.split("\r\n"))
    {
        if (line.startsWith("m=audio"))
            return line.section(' ', 3, 3).toInt();
    }
    return -1;
}

} // namespace

// Opus has no static RTP payload type, so the encoder falls back to kAudioPayloadType while
// ffmpeg's rtp muxer independently picks RTP_PT_PRIVATE + 1 for dynamic audio. getNextPacket()
// does not rewrite the payload type in the RTP header, so the two must agree or the browser sees
// a payload type its SDP never described and drops the track.
TEST(QnUniversalRtpEncoder, opusSdpMatchesRtpHeader)
{
    auto parameters = opusCodecParameters(2);

    QnUniversalRtpEncoder encoder(webRtcConfig(), nullptr);
    ASSERT_TRUE(encoder.open(parameters->getAvCodecParameters()));

    const QString sdp = encoder.getSdpMedia(/*isVideo*/ false, /*trackId*/ 0);
    EXPECT_EQ(payloadTypeFromSdpMediaLine(sdp), encoder.payloadType());

    // RFC 7587: Opus is always advertised as 48 kHz stereo regardless of the actual channel count.
    EXPECT_TRUE(sdp.contains(QString("a=rtpmap:%1 opus/48000/2").arg(encoder.payloadType())))
        << sdp.toStdString();

    // A 20 ms stereo SILK frame: TOC byte plus payload.
    uint8_t frame[80];
    memset(frame, 0x55, sizeof(frame));
    frame[0] = 0x0C;

    QnWritableCompressedAudioDataPtr audio(new QnWritableCompressedAudioData(sizeof(frame)));
    audio->compressionType = AV_CODEC_ID_OPUS;
    audio->context = parameters;
    audio->timestamp = 2'000'000;
    audio->m_data.write((const char*) frame, sizeof(frame));

    encoder.setDataPacket(audio);

    // With absoluteRtcpTimestamps the encoder emits its own RTCP sender report first; skip it.
    nx::utils::ByteArray packet;
    const nx::rtp::RtpHeader* header = nullptr;
    do
    {
        packet.clear();
        ASSERT_TRUE(encoder.getNextPacket(packet));
        ASSERT_GE((int) packet.size(), (int) nx::rtp::RtpHeader::kSize);
        header = (const nx::rtp::RtpHeader*) packet.data();
    } while (header->isRtcp());

    EXPECT_EQ((int) header->payloadType, encoder.payloadType());
}

// Adding a codec to kSrtpAudioCodecs is not enough on its own: the encoder refuses codecs outside
// isCodecSupported() and the WebRTC track is dropped rather than transcoded.
TEST(QnUniversalRtpEncoder, opusIsAccepted)
{
    for (int channels: {1, 2})
    {
        auto parameters = opusCodecParameters(channels);
        QnUniversalRtpEncoder encoder(webRtcConfig(), nullptr);
        EXPECT_TRUE(encoder.open(parameters->getAvCodecParameters())) << "channels: " << channels;
    }
}
