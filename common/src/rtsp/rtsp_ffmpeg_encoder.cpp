#include "rtsp_ffmpeg_encoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/av_codec_media_context.h>
#include <network/ffmpeg_sdp.h>
#include <nx/streaming/rtsp_client.h>
#include <nx/streaming/rtp_stream_parser.h>
#include <utils/common/util.h>
#include <nx/network/socket.h>

namespace {
    static const int kMaxPacketLen = 1024 * 32;
}

QnRtspFfmpegEncoder::QnRtspFfmpegEncoder():
    m_gotLivePacket(false),
    m_curDataBuffer(0),
    m_liveMarker(0),
    m_additionFlags(0),
    m_eofReached(false),
    m_isLastDataContext(false)
{
    // Do nothing.
}

void QnRtspFfmpegEncoder::setDstResolution(const QSize& dstVideSize, AVCodecID dstCodec)
{
    m_videoTranscoder.reset(new QnFfmpegVideoTranscoder(dstCodec));
    m_videoTranscoder->setResolution(dstVideSize);
}

void QnRtspFfmpegEncoder::init()
{
    m_contextSent.reset();
    m_gotLivePacket = false;
    m_eofReached = false;
}

QnConstMediaContextPtr QnRtspFfmpegEncoder::getGeneratedContext(AVCodecID compressionType)
{
    QMap<AVCodecID, QnConstMediaContextPtr>::iterator itr = m_generatedContexts.find(compressionType);
    QnConstMediaContextPtr result;
    if (itr != m_generatedContexts.end())
    {
        result = itr.value();
    }
    else
    {
        result = QnConstMediaContextPtr(new QnAvCodecMediaContext(compressionType));
        m_generatedContexts.insert(compressionType, result);
    }

    NX_ASSERT(result);
    return result;
}

QnConstAbstractMediaDataPtr QnRtspFfmpegEncoder::transcodeVideoPacket(QnConstAbstractMediaDataPtr media)
{
    QnAbstractMediaDataPtr transcodedMedia;
    m_videoTranscoder->transcodePacket(media, &transcodedMedia);
    if (!transcodedMedia)
        return QnConstAbstractMediaDataPtr();
    transcodedMedia->opaque = media->opaque;
    if (media->flags & QnAbstractMediaData::MediaFlags_LIVE)
        transcodedMedia->flags |= QnAbstractMediaData::MediaFlags_LIVE;
    return transcodedMedia;
}

void QnRtspFfmpegEncoder::setDataPacket(QnConstAbstractMediaDataPtr media)
{
    if (m_videoTranscoder && media->dataType == QnAbstractMediaData::VIDEO)
        media = transcodeVideoPacket(media);
    if (!media)
        return;

    m_media = media;
    m_curDataBuffer = media->data();
    m_codecCtxData.clear();
    if (m_media->flags & QnAbstractMediaData::MediaFlags_AfterEOF)
        m_contextSent.reset();

    QnConstMetaDataV1Ptr metadata = std::dynamic_pointer_cast<const QnMetaDataV1>(m_media);
    if (!metadata && m_media->compressionType)
    {
        QnConstMediaContextPtr currentContext = m_media->context;
        if (!currentContext)
            currentContext = getGeneratedContext(m_media->compressionType);
        NX_ASSERT(currentContext);
        //int rtpHeaderSize = 4 + RtpHeader::RTP_HEADER_SIZE;
        if (!m_contextSent || !m_contextSent->isSimilarTo(currentContext))
        {
            m_contextSent = currentContext;
            m_codecCtxData = currentContext->serialize();
        }
    }
    m_eofReached = false;
}

bool QnRtspFfmpegEncoder::getNextPacket(QnByteArray& sendBuffer)
{
    if (!m_media)
        return false;

    sendBuffer.resize(sendBuffer.size() + RtpHeader::RTP_HEADER_SIZE); // reserve space for RTP header

    if (!m_codecCtxData.isEmpty())
    {
        NX_ASSERT(!m_codecCtxData.isEmpty());
        sendBuffer.write(m_codecCtxData);
        m_codecCtxData.clear();
        m_isLastDataContext = true;
        return true;
    }
    m_isLastDataContext = false;

    quint16 flags = m_media->flags;
    flags |= m_additionFlags;

    int cseq = m_media->opaque;

    bool isLive = m_media->flags & QnAbstractMediaData::MediaFlags_LIVE;
    if (isLive) {
        cseq = m_liveMarker;
        if (!m_gotLivePacket)
            flags |= QnAbstractMediaData::MediaFlags_BOF;
        m_gotLivePacket = true;
    }

    // one video channel may has several subchannels (video combined with frames from difference codecContext)
    // max amount of subchannels is MAX_CONTEXTS_AT_VIDEO. Each channel used 2 ssrc: for data and for CodecContext

    const char* dataEnd = m_media->data() + m_media->dataSize();
    int dataRest = dataEnd - m_curDataBuffer;
    if (m_eofReached)
        return false; // no more data to send

    if (m_curDataBuffer == m_media->data())
    {
        // send data with RTP headers
        const QnCompressedVideoData* video = dynamic_cast<const QnCompressedVideoData*>(m_media.get());
        const QnAbstractCompressedMetadata* metadata =
            dynamic_cast<const QnAbstractCompressedMetadata*>(m_media.get());
#if 0
        int ffHeaderSize = RTSP_FFMPEG_GENERIC_HEADER_SIZE;
        if (video)
            ffHeaderSize += RTSP_FFMPEG_VIDEO_HEADER_SIZE;
        else if (metadata)
            ffHeaderSize += RTSP_FFMPEG_METADATA_HEADER_SIZE;
#endif
        //m_mutex.unlock();

        //buildRtspTcpHeader(rtpTcpChannel, ssrc, sendLen + ffHeaderSize, sendLen >= dataRest ? 1 : 0, media->timestamp, RTP_FFMPEG_GENERIC_CODE);
        //QnMutexLocker lock( &m_owner->getSockMutex() );
        //sendBuffer->write(m_rtspTcpHeader, sizeof(m_rtspTcpHeader));
        quint8 packetType = m_media->dataType;
        sendBuffer.write((const char*) &packetType, 1);
        quint32 timestampHigh = htonl(m_media->timestamp >> 32);
        sendBuffer.write((const char*) &timestampHigh, 4);
        quint8 cseq8 = cseq;
        sendBuffer.write((const char*) &cseq8, 1);
        flags = htons(flags);
        sendBuffer.write((const char*) &flags, 2);
        if (video)
        {
            quint32 videoHeader = htonl(video->dataSize() & 0x00ffffff);
            sendBuffer.write(((const char*) &videoHeader)+1, 3);
        }
        else if (metadata) {
            quint32 metadataHeader = htonl(metadata->m_duration/1000);
            quint32 metadataType = htonl(static_cast<quint32>(metadata->metadataType));
            sendBuffer.write((const char*) &metadataHeader, 4);
            sendBuffer.write((const char*) &metadataType, 4);
        }
    }

    int sendLen = qMin(int(kMaxPacketLen - sendBuffer.size()), dataRest);
    sendBuffer.write(m_curDataBuffer, sendLen);
    m_curDataBuffer += sendLen;

    m_eofReached = m_curDataBuffer == dataEnd;

    return true;
}

quint32 QnRtspFfmpegEncoder::getSSRC()
{
    return BASIC_FFMPEG_SSRC + (m_isLastDataContext ? 1 : 0);
}

bool QnRtspFfmpegEncoder::getRtpMarker()
{
    int dataRest = m_media->data() + static_cast<int>(m_media->dataSize()) - m_curDataBuffer;
    return m_isLastDataContext || dataRest == 0;
}

quint32 QnRtspFfmpegEncoder::getFrequency()
{
    return 1000000;
}

quint8 QnRtspFfmpegEncoder::getPayloadtype()
{
    return RTP_FFMPEG_GENERIC_CODE;
}

QByteArray QnRtspFfmpegEncoder::getAdditionSDP( const std::map<QString, QString>& /*streamParams*/ )
{
    if (m_codecCtxData.isEmpty())
        return QByteArray();

    return lit("a=fmtp:%1 config=%2\r\n")
        .arg(getPayloadtype())
        .arg(QLatin1String(m_codecCtxData.toBase64())).toUtf8();
}

void QnRtspFfmpegEncoder::setCodecContext(const QnConstMediaContextPtr& codecContext)
{
    if (codecContext)
        m_codecCtxData = codecContext->serialize();
}

QString QnRtspFfmpegEncoder::getName()
{
    return RTP_FFMPEG_GENERIC_STR;
}

void QnRtspFfmpegEncoder::setLiveMarker(int value)
{
    m_liveMarker = value;
}

void QnRtspFfmpegEncoder::setAdditionFlags(quint16 value)
{
    m_additionFlags = value;
}

#endif // ENABLE_DATA_PROVIDERS
