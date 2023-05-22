// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mpeg4_rtp_parser.h"

#include <nx/codec/mpeg4/utils.h>
#include <nx/utils/log/log.h>

namespace {

static constexpr uint8_t kStartCodeVos[] = {0x00, 0x00, 0x01, 0xB0}; //< Video object sequence startcode.

bool isKeyFrame(const uint8_t* data, int64_t size)
{
    int startCodeSize = sizeof(kStartCodeVos);
    return size >= startCodeSize && memcmp(data, kStartCodeVos, startCodeSize) == 0;
}

}

namespace nx::rtp {

Mpeg4Parser::Mpeg4Parser()
{
    StreamParser::setFrequency(90'000);
}

void Mpeg4Parser::setSdpInfo(const Sdp::Media& sdp)
{
    if (sdp.rtpmap.clockRate > 0)
        StreamParser::setFrequency(sdp.rtpmap.clockRate);

    for (const auto& parameter: sdp.fmtp.params)
    {
        if (parameter.startsWith("config"))
        {
            auto codecParameters = std::make_shared<CodecParameters>();
            auto avCodecParams = codecParameters->getAvCodecParameters();
            avCodecParams->codec_type = AVMEDIA_TYPE_VIDEO;
            avCodecParams->codec_id = AV_CODEC_ID_MPEG4;
            QByteArray config = QByteArray::fromHex(parameter.right(sizeof("config=")).toUtf8());
            codecParameters->setExtradata((const uint8_t*)config.data(), config.size());
            m_codecParameters = codecParameters;
        }
    }
}

Result Mpeg4Parser::processData(
    const RtpHeader& rtpHeader,
    quint8* rtpBufferBase,
    int bufferOffset,
    int bytesRead,
    bool& gotData)
{
    gotData = false;
    if (!m_packet)
    {
        m_packet = QnWritableCompressedVideoDataPtr(new QnWritableCompressedVideoData(bytesRead));
        m_packet->compressionType = AV_CODEC_ID_MPEG4;

        if (isKeyFrame(rtpBufferBase + bufferOffset, bytesRead))
        {
            m_packet->flags = QnAbstractMediaData::MediaFlags_AVKey;
            auto vol = nx::media::mpeg4::findAndParseVolHeader(
                rtpBufferBase + bufferOffset, bytesRead);
            if (vol.has_value())
            {
                m_width = vol->video_object_layer_width;
                m_height = vol->video_object_layer_height;
            }
        }
        m_packet->timestamp = rtpHeader.getTimestamp();
    }
    m_packet->width = m_width;
    m_packet->height = m_height;
    m_packet->m_data.write((char*)rtpBufferBase + bufferOffset, bytesRead);
    if (rtpHeader.marker)
    {
        gotData = true;
        m_mediaData = m_packet;
        m_packet.reset();
    }

    return {true};
}

void Mpeg4Parser::clear()
{
    m_packet.reset();
}

} // namespace nx::rtp
