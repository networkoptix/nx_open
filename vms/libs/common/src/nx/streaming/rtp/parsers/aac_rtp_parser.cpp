#include "aac_rtp_parser.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QByteArray>

#include <utils/media/bitStream.h>
#include <nx/streaming/rtp/rtp.h>
#include <nx/streaming/audio_data_packet.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/av_codec_media_context.h>
#include <nx/streaming/config.h>
#include <decoders/audio/aac.h>

namespace nx::streaming::rtp {

struct FmtpParam
{
    bool parse(const QString& str)
    {
        int valuePos = str.indexOf('=');
        if (valuePos == -1)
            return false;

        name = str.left(valuePos);
        value = str.mid(valuePos + 1);
        return true;
    }

    void getValueInt(const QString& paramName, int& result)
    {
        if (name.toLower() == paramName.toLower())
            result = value.toInt();
    }

    void getValueHex(const QString& paramName, QByteArray& result)
    {
        if (name.toLower() == paramName.toLower())
            result = QByteArray::fromHex(value.toUtf8());
    }

    QString name;
    QString value;
};

AacParser::AacParser()
{
    StreamParser::setFrequency(16000);
    m_audioLayout.reset(new QnRtspAudioLayout());
}

void AacParser::setSdpInfo(const Sdp::Media& sdp)
{
    // determine here:
    // 1. sizeLength(au size in bits)  or constantSize

    if (sdp.rtpmap.clockRate > 0)
        StreamParser::setFrequency(sdp.rtpmap.clockRate);
    if (sdp.rtpmap.channels > 0)
        m_channels = sdp.rtpmap.channels;

    QByteArray aacConfig;
    for(const QString& paramStr: sdp.fmtp.params)
    {
        FmtpParam param;
        if (!param.parse(paramStr))
            continue;

        param.getValueInt("sizeLength", m_sizeLength);
        param.getValueInt("indexLength", m_indexLength);
        param.getValueInt("indexDeltaLength", m_indexDeltaLength);
        param.getValueInt("CTSDeltaLength", m_CTSDeltaLength);
        param.getValueInt("DTSDeltaLength", m_DTSDeltaLength);
        param.getValueInt("randomAccessIndication", m_randomAccessIndication);
        param.getValueInt("streamStateIndication", m_streamStateIndication);
        param.getValueInt("profile", m_profile);
        param.getValueInt("bitrate", m_bitrate);
        param.getValueInt("streamtype", m_streamtype);
        param.getValueInt("constantSize", m_constantSize);
        param.getValueHex("config", aacConfig);
    }
    m_auHeaderExists = m_sizeLength || m_indexLength || m_indexDeltaLength || m_CTSDeltaLength ||
        m_DTSDeltaLength || m_randomAccessIndication || m_streamStateIndication;
    AACCodec aacCodec;
    aacCodec.readConfig(aacConfig);

    const auto context = new QnAvCodecMediaContext(AV_CODEC_ID_AAC);
    m_context = QnConstMediaContextPtr(context);
    const auto av = context->getAvCodecContext();
    av->channels = aacCodec.m_channels;
    av->sample_rate = aacCodec.m_sample_rate;
    av->sample_fmt = AV_SAMPLE_FMT_FLTP;
    av->time_base.num = 1;
    av->time_base.den = aacCodec.m_sample_rate;
    context->setExtradata((const quint8*) aacConfig.data(), aacConfig.size());

    QnResourceAudioLayout::AudioTrack track;
    track.codecContext = m_context;
    m_audioLayout->setAudioTrackInfo(track);

}

bool AacParser::processData(quint8* rtpBufferBase, int bufferOffset, int bufferSize, bool& gotData)
{
    gotData = false;
    const quint8* rtpBuffer = rtpBufferBase + bufferOffset;
    QVector<int> auSize;
    QVector<int> auIndex;
    QVector<int> auCtsDelta;
    QVector<int> auDtsDelta;

    RtpHeader* rtpHeader = (RtpHeader*) rtpBuffer;
    const quint8* curPtr = rtpBuffer + RtpHeader::kSize;
    const quint8* end = rtpBuffer + bufferSize;

    if (rtpHeader->extension)
    {
        if (bufferSize < RtpHeader::kSize + 4)
            return false;

        int extWords = ((int(curPtr[2]) << 8) + curPtr[3]);
        curPtr += extWords*4 + 4;
    }

    if (curPtr >= end)
        return false;
    if (rtpHeader->padding)
        end -= end[-1];
    if (curPtr >= end)
        return false;

    try
    {
        if (m_auHeaderExists)
        {
            if (end - curPtr < 2)
                return false;
            unsigned auHeaderLen = (curPtr[0] << 8) + curPtr[1];
            curPtr += 2;
            if (curPtr + auHeaderLen > end)
                return false;
            BitStreamReader reader(curPtr, curPtr + auHeaderLen);
            while(reader.getBitsCount() < auHeaderLen)
            {
                if (m_sizeLength)
                    auSize << reader.getBits(m_sizeLength);
                if (m_indexLength) {
                    if (auIndex.isEmpty())
                        auIndex << reader.getBits(m_indexLength);
                    else
                        auIndex << auIndex.last() + reader.getBits(m_indexDeltaLength) + 1;
                }
                if (m_CTSDeltaLength) {
                    if (reader.getBit())
                        auCtsDelta << reader.getBits(m_CTSDeltaLength);
                    else
                        auCtsDelta << 0;
                }
                if (m_DTSDeltaLength) {
                    if (reader.getBit())
                        auDtsDelta << reader.getBits(m_DTSDeltaLength);
                    else
                        auDtsDelta << 0;
                }
                if (m_randomAccessIndication)
                    reader.skipBit();
                if (m_streamStateIndication)
                    reader.skipBits(m_streamStateIndication);
            }
            curPtr += qPower2Ceil(auHeaderLen, 8)/8;
        }
    }
    catch (const BitStreamException&)
    {
        return false;
    }

    quint32 rtpTime = ntohl(rtpHeader->timestamp);
    for (int i = 0; curPtr < end; ++i)
    {
        int unitSize = m_constantSize;
        if (!m_constantSize)
        {
            if (auSize.size() <= i)
                return false;
            unitSize = auSize[i];
        }
        if (curPtr + unitSize > end)
            return false;

        int rtpTimeOffset = 0;
        if (auIndex.size() > i)
            rtpTimeOffset = auIndex[i];

        QnWritableCompressedAudioDataPtr audioData(
            new QnWritableCompressedAudioData(CL_MEDIA_ALIGNMENT, unitSize));
        audioData->compressionType = AV_CODEC_ID_AAC;
        audioData->context = m_context;
        audioData->timestamp = rtpTime + rtpTimeOffset;
        audioData->m_data.write((const char*)curPtr, unitSize);
        gotData = true;
        curPtr += unitSize;
        m_audioData.push_back(audioData);
    }

    return true;
}

QnConstResourceAudioLayoutPtr AacParser::getAudioLayout()
{
    return m_audioLayout;
}

} // namespace nx::streaming::rtp

#endif // ENABLE_DATA_PROVIDERS
