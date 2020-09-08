#include "mpeg12_audio_rtp_parser.h"

#include <nx/streaming/rtp/rtp.h>
#include <nx/streaming/rtp/parsers/mpeg12_audio_header_parser.h>
#include <nx/streaming/audio_data_packet.h>
#include <nx/streaming/av_codec_media_context.h>

namespace nx::streaming::rtp {

static const int kAudioHeaderSize = 4;
static const int kAudioHeaderMbzFieldSize = 2;
static const int kAudioHeaderFragmentOffsetFieldSize = 2;

AVCodecID codecIdFromMpegLayer(MpegLayer mpegLayer)
{
    switch (mpegLayer)
    {
        case MpegLayer::layer1: return AV_CODEC_ID_MP1;
        case MpegLayer::layer2: return AV_CODEC_ID_MP2;
        case MpegLayer::layer3: return AV_CODEC_ID_MP3;
        default: return AV_CODEC_ID_NONE;
    }
}

int channelNumberFromChannelMode(MpegChannelMode mpegChannelMode)
{
    switch (mpegChannelMode)
    {
        case MpegChannelMode::dualChannel: [[fallthrough]];
        case MpegChannelMode::jointStereo: [[fallthrough]];
        case MpegChannelMode::stereo: return 2;
        case MpegChannelMode::singleChannel: return 1;
        default: return 1;
    }
}

Mpeg12AudioParser::Mpeg12AudioParser()
{
    m_audioLayout.reset(new QnRtspAudioLayout());
}

void Mpeg12AudioParser::setSdpInfo(const Sdp::Media& sdp)
{
    setFrequency(sdp.rtpmap.clockRate);
}

StreamParser::Result Mpeg12AudioParser::processData(
    quint8* rtpBufferBase,
    int bufferOffset,
    int bytesRead,
    bool& gotData)
{
    static const int kMinimalPacketSize = RtpHeader::kSize + kAudioHeaderSize + 1;
    if (bytesRead < kMinimalPacketSize)
    {
        cleanUp();
        return {
            false,
            NX_FMT("Failed to parse RTP packet, invalid RTP packet size: %1, expected at least %2",
                bytesRead, kMinimalPacketSize) };
    }

    const uint8_t* currentPtr = rtpBufferBase + bufferOffset;
    const auto rtpHeader = (RtpHeader*)currentPtr;
    const uint32_t timestamp = ntohl(rtpHeader->timestamp);

    if (m_rtpTimestamp == 0)
        m_rtpTimestamp = timestamp;

    if (!m_audioDataChunks.empty() && (timestamp != m_rtpTimestamp))
    {
        createAudioFrame();
        gotData = true;
        m_rtpTimestamp = timestamp;
    }

    currentPtr += RtpHeader::kSize + kAudioHeaderMbzFieldSize;
    const int fragmentOffset = *(uint16_t*)(currentPtr);
    currentPtr += kAudioHeaderFragmentOffsetFieldSize;
    const int payloadSize = bytesRead - RtpHeader::kSize - kAudioHeaderSize;

    if (fragmentOffset == 0)
    {
        m_skipFragmentsUntilNextAudioFrame = false;
        const StreamParser::Result result = updateFromMpegAudioHeader(currentPtr, payloadSize);
        if (!result.success)
        {
            m_skipFragmentsUntilNextAudioFrame = true;
            return result;
        }
    }

    if (fragmentOffset == 0 || !m_skipFragmentsUntilNextAudioFrame)
        m_audioDataChunks.push_back({ currentPtr, payloadSize });

    return {true};
}

StreamParser::Result Mpeg12AudioParser::updateFromMpegAudioHeader(
    const uint8_t* mpegAudioHeaderStart, int payloadSize)
{
    const std::optional<Mpeg12AudioHeader> mpegAudioHeader =
        Mpeg12AudioHeaderParser::parse(mpegAudioHeaderStart, payloadSize);

    if (!mpegAudioHeader)
        return {false, "Unable to parse MPEG audio header"};

    const AVCodecID codecId = codecIdFromMpegLayer(mpegAudioHeader->layer);
    if (!NX_ASSERT(codecId != AV_CODEC_ID_NONE))
        return {false, "Unable to determine codec"};

    auto context = new QnAvCodecMediaContext(codecId);
    m_mediaContext = QnConstMediaContextPtr(context);

    AVCodecContext* const avCodecContext = context->getAvCodecContext();
    avCodecContext->channels = channelNumberFromChannelMode(mpegAudioHeader->channelMode);
    avCodecContext->sample_rate = mpegAudioHeader->samplingRate;

    QnResourceAudioLayout::AudioTrack track;
    track.codecContext = m_mediaContext;
    m_audioLayout->setAudioTrackInfo(track);

    return {true};
}

QnConstResourceAudioLayoutPtr Mpeg12AudioParser::getAudioLayout()
{
    return m_audioLayout;
}

void Mpeg12AudioParser::createAudioFrame()
{
    QnWritableCompressedAudioDataPtr audioFrame(
        new QnWritableCompressedAudioData(CL_MEDIA_ALIGNMENT, m_audioFrameSize));

    for (const DataChunk& dataChunk: m_audioDataChunks)
        audioFrame->m_data.write((const char*)dataChunk.start, dataChunk.length);

    audioFrame->compressionType = m_mediaContext->getCodecId();
    audioFrame->context = m_mediaContext;
    audioFrame->timestamp = m_rtpTimestamp;

    cleanUp();

    m_audioData.push_back(std::move(audioFrame));
}

void Mpeg12AudioParser::cleanUp()
{
    m_audioFrameSize = 0;
    m_audioDataChunks.clear();
}

} // namespace nx::streaming::rtp