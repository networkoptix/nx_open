#include "rtsp_ffmpeg_encoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/av_codec_media_context.h>
#include <utils/common/util.h>
#include <nx/network/socket.h>
#include <nx/streaming/rtp/rtp.h>

namespace {
    static const int kMaxPacketLen = 1024 * 32;
}

static const int kNxPayloadType = 102;
static const QString kNxPayloadTypeName("FFMPEG");
static const uint32_t kNxBasicSsrc = 20000;

QnRtspFfmpegEncoder::QnRtspFfmpegEncoder(const DecoderConfig& config, nx::metrics::Storage* metrics)
    :
    m_gotLivePacket(false),
    m_curDataBuffer(0),
    m_liveMarker(0),
    m_additionFlags(0),
    m_eofReached(false),
    m_metrics(metrics)
{
    // Do nothing.
}

void QnRtspFfmpegEncoder::setDstResolution(const QSize& dstVideSize, AVCodecID dstCodec)
{
    m_videoTranscoder.reset(new QnFfmpegVideoTranscoder(m_config, m_metrics, dstCodec));
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

    bool hasDataContext = !m_codecCtxData.isEmpty();
    bool rtpMarker = hasDataContext;
    uint32_t ssrc = kNxBasicSsrc + (hasDataContext ? 1 : 0);

    int dataStartIndex = sendBuffer.size();
    sendBuffer.resize(sendBuffer.size() + nx::streaming::rtp::RtpHeader::kSize);
    nx::streaming::rtp::buildRtpHeader(
        sendBuffer.data() + dataStartIndex,
        ssrc,
        rtpMarker,
        m_media->timestamp,
        kNxPayloadType,
        m_sequence++);

    if (!m_codecCtxData.isEmpty())
    {
        NX_ASSERT(!m_codecCtxData.isEmpty());
        sendBuffer.write(m_codecCtxData);
        m_codecCtxData.clear();
        return true;
    }

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

    // One video channel may has several subchannels (video combined with frames from
    // difference codecContext). Max amount of subchannels is MAX_CONTEXTS_AT_VIDEO. Each channel
    // used 2 ssrc: for data and for CodecContext.

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
    nx::streaming::rtp::RtpHeader* rtpHeader =
        (nx::streaming::rtp::RtpHeader*)(sendBuffer.data() + dataStartIndex);
    rtpHeader->marker = m_eofReached;
    return true;
}

QString QnRtspFfmpegEncoder::getSdpMedia(bool isVideo, int trackId)
{
    QString sdpMedia;
    QTextStream stream(&sdpMedia);
    stream << "m=" << (isVideo ? "video " : "audio ") << 0 << " RTP/AVP ";
    stream << kNxPayloadType << "\r\n";
    stream << "a=control:trackID=" << trackId << "\r\n";
    stream << "a=rtpmap:" << kNxPayloadType << " " << kNxPayloadTypeName << "/" << 1000000 <<"\r\n";

    if (!m_codecCtxData.isEmpty())
        stream << "a=fmtp:" << kNxPayloadType << " config=" << m_codecCtxData.toBase64() <<"\r\n";

    return sdpMedia;
}

void QnRtspFfmpegEncoder::setCodecContext(const QnConstMediaContextPtr& codecContext)
{
    if (codecContext)
        m_codecCtxData = codecContext->serialize();
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
