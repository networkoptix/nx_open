#include "ffmpeg_rtp_parser.h"
#include "rtp_stream_parser.h"
#include "rtpsession.h"

QnFfmpegRtpParser::QnFfmpegRtpParser(): QnRtpVideoStreamParser(), m_position(AV_NOPTS_VALUE)
{

}

QnFfmpegRtpParser::~QnFfmpegRtpParser()
{

}

void QnFfmpegRtpParser::setSDPInfo(QList<QByteArray>)
{

}

bool QnFfmpegRtpParser::processData(quint8* rtpBufferBase, int bufferOffset, int dataSize, const RtspStatistic&, QnAbstractMediaDataPtr& result)
{
    if (dataSize < RtpHeader::RTP_HEADER_SIZE)
    {
        qWarning() << Q_FUNC_INFO << __LINE__ << "strange RTP packet. len < RTP header size. Ignored";
        return false;
    }
    const quint8* data = rtpBufferBase + bufferOffset;
    RtpHeader* rtpHeader = (RtpHeader*) data;
    const quint8* payload = data + RtpHeader::RTP_HEADER_SIZE;
    dataSize -= RtpHeader::RTP_HEADER_SIZE;
    const bool isCodecContext = ntohl(rtpHeader->ssrc) & 1; // odd numbers - codec context, even numbers - data
    if (isCodecContext)
    {
        QnMediaContextPtr context(new QnMediaContext(payload, dataSize));
        m_context = context;
    }
    else
    {
        QnMediaContextPtr context = m_context;

        if (rtpHeader->padding)
            dataSize -= ntohl(rtpHeader->padding);
        if (dataSize < 1)
            return false;

        if (!m_nextDataPacket)
        {
            if (dataSize < RTSP_FFMPEG_GENERIC_HEADER_SIZE)
                return false;

            QnAbstractMediaData::DataType dataType = (QnAbstractMediaData::DataType) *payload++;
            quint32 timestampHigh = ntohl(*(quint32*) payload);
            dataSize -= RTSP_FFMPEG_GENERIC_HEADER_SIZE;
            payload  += 4; // deserialize timeStamp high part

            quint8 cseq = *payload++;
            quint16 flags = (payload[0]<<8) + payload[1];
            payload += 2;

            if (dataType == QnAbstractMediaData::EMPTY_DATA)
            {
                m_nextDataPacket = QnEmptyMediaDataPtr(new QnEmptyMediaData());
                if (dataSize != 0)
                {
                    qWarning() << "Unexpected data size for EOF/BOF packet. got" << dataSize << "expected" << 0 << "bytes. Packet ignored.";
                    return false;
                }
            }
            else if (dataType == QnAbstractMediaData::META_V1)
            {
                if (dataSize != MD_WIDTH*MD_HEIGHT/8 + RTSP_FFMPEG_METADATA_HEADER_SIZE)
                {
                    qWarning() << "Unexpected data size for metadata. got" << dataSize << "expected" << MD_WIDTH*MD_HEIGHT/8 << "bytes. Packet ignored.";
                    return false;
                }
                if (dataSize < RTSP_FFMPEG_METADATA_HEADER_SIZE)
                    return result;

                QnMetaDataV1 *metadata = new QnMetaDataV1(); 
                metadata->data.clear();
                context.clear();
                metadata->m_duration = ntohl(*((quint32*)payload))*1000;
                dataSize -= RTSP_FFMPEG_METADATA_HEADER_SIZE;
                payload += RTSP_FFMPEG_METADATA_HEADER_SIZE; // deserialize video flags

                m_nextDataPacket = QnMetaDataV1Ptr(metadata);
            }
            else if (context && context->ctx() && context->ctx()->codec_type == AVMEDIA_TYPE_VIDEO && dataType == QnAbstractMediaData::VIDEO)
            {
                if (dataSize < RTSP_FFMPEG_VIDEO_HEADER_SIZE)
                    return false;

                quint32 fullPayloadLen = (payload[0] << 16) + (payload[1] << 8) + payload[2];

                dataSize -= RTSP_FFMPEG_VIDEO_HEADER_SIZE;
                payload += RTSP_FFMPEG_VIDEO_HEADER_SIZE; // deserialize video flags

                QnCompressedVideoData *video = new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, fullPayloadLen, context);
                m_nextDataPacket = QnCompressedVideoDataPtr(video);
                if (context) 
                {
                    video->width = context->ctx()->coded_width;
                    video->height = context->ctx()->coded_height;
                }
            }
            else if (context && context->ctx() && context->ctx()->codec_type == AVMEDIA_TYPE_AUDIO && dataType == QnAbstractMediaData::AUDIO)
            {
                QnCompressedAudioData *audio = new QnCompressedAudioData(CL_MEDIA_ALIGNMENT, dataSize); // , context
                audio->context = context;
                //audio->format.fromAvStream(context->ctx());
                m_nextDataPacket = QnCompressedAudioDataPtr(audio);
            }
            else
            {
                if (context && context->ctx())
                    qWarning() << "Unsupported RTP codec or packet type. codec=" << context->ctx()->codec_type;
                else if (dataType == QnAbstractMediaData::AUDIO)
                    qWarning() << "Unsupported audio codec or codec params";
                else if (dataType == QnAbstractMediaData::VIDEO)
                    qWarning() << "Unsupported video or codec params";
                else
                    qWarning() << "Unsupported or unknown packet";
                return false;
            }

            if (m_nextDataPacket) 
            {
                m_nextDataPacket->opaque = cseq;
                m_nextDataPacket->flags = flags;

                if (context && context->ctx())
                    m_nextDataPacket->compressionType = context->ctx()->codec_id;
                m_nextDataPacket->timestamp = ntohl(rtpHeader->timestamp) + (qint64(timestampHigh) << 32);
                //m_nextDataPacket->channelNumber = channelNum;
                /*
                if (mediaPacket->timestamp < 0x40000000 && m_prevTimestamp[ssrc] > 0xc0000000)
                    m_timeStampCycles[ssrc]++;
                mediaPacket->timestamp += ((qint64) m_timeStampCycles[ssrc] << 32);
                */
            }
        }

        if (m_nextDataPacket) {
            m_nextDataPacket->data.write((const char*)payload, dataSize);
        }

        if (rtpHeader->marker)
        {
            if (m_nextDataPacket)
            {
                if (m_nextDataPacket->flags & QnAbstractMediaData::MediaFlags_LIVE)
                    m_position = DATETIME_NOW;
                else
                    m_position = result->timestamp;
            }
            result = m_nextDataPacket;
            m_nextDataPacket.clear(); // EOF video frame reached
        }
    }
    return true;
}
