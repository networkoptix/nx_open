#include "simpleaudio_rtp_parser.h"
#include "rtp_stream_parser.h"
#include "rtpsession.h"
#include "utils/common/synctime.h"
#include "core/datapacket/media_data_packet.h"

QnSimpleAudioRtpParser::QnSimpleAudioRtpParser():
    QnRtpAudioStreamParser()
{
    m_frequency = 8000;
    m_channels = 1;
    m_codecId = CODEC_ID_PCM_MULAW;
    m_sampleFormat = AV_SAMPLE_FMT_U8;
    m_bits_per_coded_sample = 8;

    m_audioLayout.reset( new QnRtspAudioLayout() );
}

QnSimpleAudioRtpParser::~QnSimpleAudioRtpParser()
{
}

void QnSimpleAudioRtpParser::setCodecId(CodecID codecId)
{   
    m_codecId = codecId;
}

void QnSimpleAudioRtpParser::setSDPInfo(QList<QByteArray> lines)
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
                //processStringParam("mode-set", m_mode, param);
            }
        }   
    }
    
    m_context = QnMediaContextPtr(new QnMediaContext(m_codecId));
    m_context->ctx()->channels = m_channels;
    m_context->ctx()->sample_rate = m_frequency;
    m_context->ctx()->sample_fmt = m_sampleFormat;
    m_context->ctx()->time_base.num = 1;
    m_context->ctx()->bits_per_coded_sample = m_bits_per_coded_sample;
    m_context->ctx()->time_base.den = m_frequency;

    QnResourceAudioLayout::AudioTrack track;
    track.codecContext = m_context;
    m_audioLayout->setAudioTrackInfo(track);
}

bool QnSimpleAudioRtpParser::processData(quint8* rtpBufferBase, int bufferOffset, int bufferSize, const RtspStatistic& statistics, QList<QnAbstractMediaDataPtr>& result)
{
    const quint8* rtpBuffer = rtpBufferBase + bufferOffset;
    result.clear();

    RtpHeader* rtpHeader = (RtpHeader*) rtpBuffer;
    const quint8* curPtr = rtpBuffer + RtpHeader::RTP_HEADER_SIZE;
    const quint8* end = rtpBuffer + bufferSize;
    if (curPtr >= end)
        return false;

    QnCompressedAudioDataPtr audioData = QnCompressedAudioDataPtr(new QnCompressedAudioData(CL_MEDIA_ALIGNMENT, end - curPtr));
    audioData->compressionType = m_context->ctx()->codec_id;
    audioData->context = m_context;
    if (m_timeHelper) {
        audioData->timestamp = m_timeHelper->getUsecTime(ntohl(rtpHeader->timestamp), statistics, m_frequency);
        //qDebug() << "audio. adjusttime to " << (audioData->timestamp - qnSyncTime->currentMSecsSinceEpoch()*1000)/1000 << "ms";
    }
    else
        audioData->timestamp = qnSyncTime->currentMSecsSinceEpoch() * 1000;

    audioData->data.write((const char*)curPtr, end - curPtr);
    result << audioData;

    return true;
}

QnResourceAudioLayoutPtr QnSimpleAudioRtpParser::getAudioLayout()
{
    return m_audioLayout;
}

void QnSimpleAudioRtpParser::setBitsPerSample(int value)
{
    m_bits_per_coded_sample = value;
}

void QnSimpleAudioRtpParser::setSampleFormat(AVSampleFormat sampleFormat)
{
    m_sampleFormat = sampleFormat;
}
