#include "aac_rtp_parser.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <utils/common/synctime.h>
#include <utils/math/math.h>

#include <core/datapacket/audio_data_packet.h>
#include <core/datapacket/media_data_packet.h>

#include "rtp_stream_parser.h"
#include "rtpsession.h"

QnAacRtpParser::QnAacRtpParser():
    QnRtpAudioStreamParser()
{
    m_sizeLength = 0;
    m_constantSize = 0;
    m_indexLength = 0;
    m_indexDeltaLength = 0;
    m_CTSDeltaLength = 0;
    m_DTSDeltaLength = 0;
    m_randomAccessIndication = 0;
    m_streamStateIndication = 0;
    m_profile = 0;
    m_bitrate = 0;
    m_frequency = 16000;
    m_channels = 0;
    m_streamtype = 0;
    m_auHeaderExists = false;

    m_audioLayout.reset( new QnRtspAudioLayout() );
}

QnAacRtpParser::~QnAacRtpParser()
{
}

void QnAacRtpParser::setSDPInfo(QList<QByteArray> lines)
{
    // determine here:
    // 1. sizeLength(au size in bits)  or constantSize

    for (int i = 0; i < lines.size(); ++ i)
    {
        if (lines[i].startsWith("a=rtpmap"))
        {
            QList<QByteArray> params = lines[i].mid(lines[i].indexOf(' ')+1).split('/');
            if (params.size() > 1)
                m_frequency = params[1].trimmed().toInt();
            if (params.size() > 2)
                m_channels = params[2].trimmed().toInt();
        }
        else if (lines[i].startsWith("a=fmtp"))
        {
            QList<QByteArray> params = lines[i].mid(lines[i].indexOf(' ')+1).split(';');
            foreach(QByteArray param, params) 
            {
                param = param.trimmed();
                processIntParam("sizeLength", m_sizeLength, param);
                processIntParam("indexLength", m_indexLength, param);
                processIntParam("indexDeltaLength", m_indexDeltaLength, param);
                processIntParam("CTSDeltaLength", m_CTSDeltaLength, param);
                processIntParam("DTSDeltaLength", m_DTSDeltaLength, param);
                processIntParam("randomAccessIndication", m_randomAccessIndication, param);
                processIntParam("streamStateIndication", m_streamStateIndication, param);
                processIntParam("profile", m_profile, param);
                processIntParam("bitrate", m_bitrate, param);
                processIntParam("streamtype", m_streamtype, param);
                processHexParam("config", m_config, param);
                processStringParam("mode", m_mode, param);
                processIntParam("constantSize", m_constantSize, param);
            }
        }   
    }
    m_auHeaderExists = m_sizeLength || m_indexLength || m_indexDeltaLength || m_CTSDeltaLength || m_DTSDeltaLength || m_randomAccessIndication || m_streamStateIndication;
    m_aacHelper.readConfig(m_config);
    
    m_context = QnMediaContextPtr(new QnMediaContext(CODEC_ID_AAC));
    m_context->ctx()->channels = m_aacHelper.m_channels;
    m_context->ctx()->sample_rate = m_aacHelper.m_sample_rate;
    m_context->ctx()->sample_fmt = AV_SAMPLE_FMT_S16;
    m_context->ctx()->time_base.num = 1;
    m_context->ctx()->time_base.den = m_aacHelper.m_sample_rate;
    m_context->ctx()->extradata = (uint8_t*) av_malloc(m_config.size());
    memcpy(m_context->ctx()->extradata, m_config.data(), m_config.size());
    m_context->ctx()->extradata_size = m_config.size();

    QnResourceAudioLayout::AudioTrack track;
    track.codecContext = m_context;
    m_audioLayout->setAudioTrackInfo(track);

}

bool QnAacRtpParser::processData(quint8* rtpBufferBase, int bufferOffset, int bufferSize, const RtspStatistic& statistics, bool& gotData)
{
    gotData = false;
    const quint8* rtpBuffer = rtpBufferBase + bufferOffset;
    QVector<int> auSize;
    QVector<int> auIndex;
    QVector<int> auCtsDelta;
    QVector<int> auDtsDelta;
    //bool rapFlag = false;
    //int streamStateValue = 0;

    RtpHeader* rtpHeader = (RtpHeader*) rtpBuffer;
    const quint8* curPtr = rtpBuffer + RtpHeader::RTP_HEADER_SIZE;
    const quint8* end = rtpBuffer + bufferSize;

    if (rtpHeader->extension)
    {
        if (bufferSize < RtpHeader::RTP_HEADER_SIZE + 4)
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

    //bool isLastPacket = rtpHeader->marker;
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
                    //rapFlag = reader.getBit();
                    reader.skipBit();
                if (m_streamStateIndication)
                    //streamStateValue = reader.getBits(m_streamStateIndication);
                    reader.skipBits(m_streamStateIndication);
            }
            curPtr += qPower2Ceil(auHeaderLen, 8)/8;
        }
    } catch(...) {
        return false;
    }

    quint32 rtpTime = ntohl(rtpHeader->timestamp);
    for (int i = 0; curPtr < end; ++i)
    {
        int unitSize = m_constantSize;
        
        if (!m_constantSize) {
            if (auSize.size() < i+1)
                return false;
            unitSize = auSize[i];
        }
        if (curPtr + unitSize > end)
            return false;

        int rtpTimeOffset = 0;
        if (auIndex.size() > i)
            rtpTimeOffset = auIndex[i];

        QnWritableCompressedAudioDataPtr audioData = QnWritableCompressedAudioDataPtr(new QnWritableCompressedAudioData(CL_MEDIA_ALIGNMENT, unitSize));
        audioData->compressionType = CODEC_ID_AAC;
        audioData->context = m_context;
        if (m_timeHelper)
            audioData->timestamp = m_timeHelper->getUsecTime(rtpTime+rtpTimeOffset, statistics, m_frequency);
        else
            audioData->timestamp = qnSyncTime->currentMSecsSinceEpoch() * 1000;


        //quint8 adtsHeaderBuff[AAC_HEADER_LEN];
        //m_aacHelper.buildADTSHeader(adtsHeaderBuff, unitSize);
        //audioData->data.write((const char*) adtsHeaderBuff, AAC_HEADER_LEN);
        audioData->m_data.write((const char*)curPtr, unitSize);
        m_audioData.push_back(audioData);
        gotData = true;
        curPtr += unitSize;
    }

    return true;
}

QnResourceAudioLayoutPtr QnAacRtpParser::getAudioLayout()
{
    return m_audioLayout;
}

#endif // ENABLE_DATA_PROVIDERS
