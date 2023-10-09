// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rtsp_ffmpeg_encoder.h"

#include <nx/media/codec_parameters.h>
#include <nx/media/video_data_packet.h>
#include <nx/network/socket.h>
#include <nx/rtp/rtp.h>
#include <utils/common/util.h>

namespace {
    static const int kMaxPacketLen = 1024 * 32;
}

static const QString kNxPayloadTypeName("FFMPEG");
static const uint32_t kNxBasicSsrc = 20'000;

QnRtspFfmpegEncoder::QnRtspFfmpegEncoder(const DecoderConfig& config, nx::metrics::Storage* metrics)
    :
    m_config(config),
    m_gotLivePacket(false),
    m_curDataBuffer(0),
    m_liveMarker(0),
    m_additionFlags(0),
    m_eofReached(false),
    m_mtu(kMaxPacketLen),
    m_metrics(metrics)
{
    setSsrc(kNxBasicSsrc);
}

void QnRtspFfmpegEncoder::setDstResolution(const QSize& dstVideoSize, AVCodecID dstCodec)
{
    m_videoTranscoder.reset(new QnFfmpegVideoTranscoder(m_config, m_metrics, dstCodec));
    m_videoTranscoder->setOutputResolutionLimit(dstVideoSize);
}

void QnRtspFfmpegEncoder::setMtu(int mtu)
{
    m_mtu = mtu;
}

void QnRtspFfmpegEncoder::init()
{
    m_contextSent.reset();
    m_gotLivePacket = false;
    m_eofReached = false;
}

CodecParametersConstPtr QnRtspFfmpegEncoder::getGeneratedContext(AVCodecID compressionType)
{
    QMap<AVCodecID, CodecParametersConstPtr>::iterator itr = m_generatedContexts.find(compressionType);
    CodecParametersConstPtr result;
    if (itr != m_generatedContexts.end())
    {
        result = itr.value();
    }
    else
    {
        result = std::make_shared<CodecParameters>();
        result->getAvCodecParameters()->codec_id = compressionType;
        result->getAvCodecParameters()->codec_type = avcodec_get_type(compressionType);
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
    NX_VERBOSE(this, "Received media data: timestamp %1, dataType %2",
        media->timestamp, media->dataType);

    if (m_videoTranscoder && media->dataType == QnAbstractMediaData::VIDEO)
        media = transcodeVideoPacket(media);
    if (!media)
        return;

    m_media = media;
    m_curDataBuffer = media->data();
    m_codecParamsData.clear();
    if (m_media->flags & QnAbstractMediaData::MediaFlags_AfterEOF)
        m_contextSent.reset();

    QnConstMetaDataV1Ptr metadata = std::dynamic_pointer_cast<const QnMetaDataV1>(m_media);
    if (!metadata && m_media->compressionType)
    {
        CodecParametersConstPtr currentContext = m_media->context;
        if (!currentContext)
            currentContext = getGeneratedContext(m_media->compressionType);
        NX_ASSERT(currentContext);
        if (!m_contextSent || !m_contextSent->isEqual(*currentContext.get()))
        {
            m_contextSent = currentContext;
            if (!m_serverVersion.isNull() && m_serverVersion.major < 5)
                m_codecParamsData = currentContext->serializeInDeprecatedFormat42();
            else
                m_codecParamsData = currentContext->serialize();
        }
    }
    m_eofReached = false;
}

bool QnRtspFfmpegEncoder::getNextPacket(nx::utils::ByteArray& sendBuffer)
{
    if (m_eofReached || !m_media)
        return false;

    bool hasDataContext = !m_codecParamsData.isEmpty();
    bool rtpMarker = hasDataContext;
    uint32_t ssrc = *m_ssrc + (hasDataContext ? 1 : 0);

    int dataStartIndex = sendBuffer.size();
    sendBuffer.resize(sendBuffer.size() + nx::rtp::RtpHeader::kSize);
    nx::rtp::buildRtpHeader(
        sendBuffer.data() + dataStartIndex,
        ssrc,
        rtpMarker,
        m_media->timestamp,
        nx::rtp::kNxPayloadType,
        m_sequence++);

    if (!m_codecParamsData.isEmpty())
    {
        NX_ASSERT(!m_codecParamsData.isEmpty());
        sendBuffer.write(m_codecParamsData);
        m_codecParamsData.clear();
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

    const bool isFirstPacketForFrame = m_curDataBuffer == m_media->data();
    if (isFirstPacketForFrame)
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

    const char* const dataEnd = m_media->data() + m_media->dataSize();
    int sendLen = qMin<int>(m_mtu - sendBuffer.size(), dataEnd - m_curDataBuffer);
    sendBuffer.write(m_curDataBuffer, sendLen);
    m_curDataBuffer += sendLen;

    m_eofReached = m_curDataBuffer == dataEnd;
    nx::rtp::RtpHeader* rtpHeader =
        (nx::rtp::RtpHeader*)(sendBuffer.data() + dataStartIndex);
    rtpHeader->marker = m_eofReached;

    NX_VERBOSE(this, "Made RTP packet: isFirst %1, timestamp %2, sequence %3",
        isFirstPacketForFrame ? "true" : "false",
        m_media->timestamp,
        qFromBigEndian(rtpHeader->sequence));

    return true;
}

QString QnRtspFfmpegEncoder::getSdpMedia(bool isVideo, int trackId, int port)
{
    QString sdpMedia;
    QTextStream stream(&sdpMedia);
    stream << "m=" << (isVideo ? "video " : "audio ") << port << " RTP/AVP ";
    stream << nx::rtp::kNxPayloadType << "\r\n";
    stream << "a=control:trackID=" << trackId << "\r\n";
    stream << "a=rtpmap:" << nx::rtp::kNxPayloadType << " " << kNxPayloadTypeName << "/" << 1'000'000 <<"\r\n";

    if (!m_codecParamsData.isEmpty())
        stream << "a=fmtp:" << nx::rtp::kNxPayloadType << " config=" << m_codecParamsData.toBase64() <<"\r\n";

    return sdpMedia;
}

void QnRtspFfmpegEncoder::setCodecContext(const CodecParametersConstPtr& codecParams)
{
    if (codecParams)
        m_codecParamsData = codecParams->serialize();
}

void QnRtspFfmpegEncoder::setLiveMarker(int value)
{
    m_liveMarker = value;
}

void QnRtspFfmpegEncoder::setAdditionFlags(quint16 value)
{
    m_additionFlags = value;
}

void QnRtspFfmpegEncoder::setServerVersion(const nx::utils::SoftwareVersion& serverVersion)
{
    m_serverVersion = serverVersion;
}
