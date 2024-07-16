// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "aac_rtp_parser.h"

#include <QtCore/QByteArray>

#include <nx/codec/aac/aac.h>
#include <nx/media/audio_data_packet.h>
#include <nx/media/codec_parameters.h>
#include <nx/media/config.h>
#include <nx/media/media_data_packet.h>
#include <nx/rtp/rtp.h>
#include <nx/utils/bit_stream.h>
#include <nx/utils/math/math.h>

namespace nx::rtp {

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

    m_context = std::make_shared<CodecParameters>();
    const auto codecParams = m_context->getAvCodecParameters();
    codecParams->codec_type = AVMEDIA_TYPE_AUDIO;
    codecParams->codec_id = AV_CODEC_ID_AAC;
    av_channel_layout_default(&codecParams->ch_layout, aacCodec.m_channels);
    codecParams->sample_rate = aacCodec.m_sample_rate;
    codecParams->format = AV_SAMPLE_FMT_FLTP;
    m_context->setExtradata((const quint8*)aacConfig.data(), aacConfig.size());
}

Result AacParser::processData(
    const RtpHeader& rtpHeader, quint8* rtpBufferBase, int bufferOffset, int bufferSize, bool& gotData)
{
    gotData = false;

    QVector<int> auSize;
    QVector<int> auIndex;
    QVector<int> auCtsDelta;
    QVector<int> auDtsDelta;

    const quint8* rtpBuffer = rtpBufferBase + bufferOffset;
    const quint8* curPtr = rtpBuffer;
    const quint8* end = rtpBuffer + bufferSize;

    if (curPtr >= end)
        return {false, "Malformed AAC packet"};

    try
    {
        if (m_auHeaderExists)
        {
            if (end - curPtr < 2)
                return {false, "Malformed AAC packet. Not enough data to parse buffer"};
            unsigned auHeaderLen = (curPtr[0] << 8) + curPtr[1];
            curPtr += 2;
            if (curPtr + auHeaderLen > end)
            {
                return {false, NX_FMT(
                    "Malformed AAC packet. Invalid au header length %1", auHeaderLen)};
            }
            nx::utils::BitStreamReader reader(curPtr, curPtr + auHeaderLen);
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
    catch (const nx::utils::BitStreamException&)
    {
        return {false, "Malformed AAC packet. Bitstream parser error."};
    }

    quint32 rtpTime = rtpHeader.getTimestamp();
    for (int i = 0; curPtr < end; ++i)
    {
        int unitSize = m_constantSize;
        if (!m_constantSize)
        {
            if (auSize.size() <= i)
            {
                return {false, NX_FMT("Malformed AAC packet. Invalid auSize header. "
                    "auSize %1, expected at least %2", auSize.size(), i + 1)};
            }
            unitSize = auSize[i];
        }
        if (curPtr + unitSize > end)
        {
            return {false, NX_FMT("Malformed AAC packet. Invalid unit size %1 bytes.", unitSize)};
        }

        int rtpTimeOffset = 0;
        if (auIndex.size() > i)
            rtpTimeOffset = auIndex[i];

        QnWritableCompressedAudioDataPtr audioData(new QnWritableCompressedAudioData(unitSize));
        audioData->compressionType = AV_CODEC_ID_AAC;
        audioData->context = m_context;
        audioData->timestamp = rtpTime + rtpTimeOffset;
        audioData->m_data.write((const char*)curPtr, unitSize);
        gotData = true;
        curPtr += unitSize;
        m_audioData.push_back(audioData);
    }

    return {true};
}

CodecParametersConstPtr AacParser::getCodecParameters()
{
    return m_context;
}

} // namespace nx::rtp
