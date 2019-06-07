#include "rtp_universal_encoder.h"

#include <array>

#include <nx/streaming/config.h>
#include <nx/streaming/rtp/onvif_header_extension.h>
#include "common/common_module.h"

#include <core/resource/param.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

static const int kVideoPayloadType = 96;
static const int kAudioPayloadType = 97;
static const int kSecondaryStreamPayloadType = 98;

/* from http://www.iana.org/assignments/rtp-parameters last updated 05 January 2005 */
/* payload types >= 96 are dynamic;
 * payload types between 72 and 76 are reserved for RTCP conflict avoidance;
 * all the other payload types not present in the table are unassigned or
 * reserved
 */
struct AVRtpPayloadType
{
    int pt;
    const char enc_name[14];
    AVMediaType codec_type;
    AVCodecID codec_id;
    int clock_rate;
    int audio_channels;
};

std::array<AVRtpPayloadType, 29> kStaticPayloadTypes = {{
    {0, "PCMU",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_PCM_MULAW, 8000, 1},
    {3, "GSM",         AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_NONE, 8000, 1},
    {4, "G723",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_G723_1, 8000, 1},
    {5, "DVI4",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_NONE, 8000, 1},
    {6, "DVI4",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_NONE, 16000, 1},
    {7, "LPC",         AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_NONE, 8000, 1},
    {8, "PCMA",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_PCM_ALAW, 8000, 1},
    {9, "G722",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_ADPCM_G722, 8000, 1},
    {10, "L16",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_PCM_S16BE, 44100, 2},
    {11, "L16",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_PCM_S16BE, 44100, 1},
    {12, "QCELP",      AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_QCELP, 8000, 1},
    {13, "CN",         AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_NONE, 8000, 1},
    {14, "MPA",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_MP2, -1, -1},
    {97, "mpa-robust", AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_MP3, -1, -1},
    {15, "G728",       AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_NONE, 8000, 1},
    {16, "DVI4",       AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_NONE, 11025, 1},
    {17, "DVI4",       AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_NONE, 22050, 1},
    {18, "G729",       AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_NONE, 8000, 1},
    {25, "CelB",       AVMEDIA_TYPE_VIDEO,   AV_CODEC_ID_NONE, 90000, -1},
    {26, "JPEG",       AVMEDIA_TYPE_VIDEO,   AV_CODEC_ID_MJPEG, 90000, -1},
    {28, "nv",         AVMEDIA_TYPE_VIDEO,   AV_CODEC_ID_NONE, 90000, -1},
    {31, "H261",       AVMEDIA_TYPE_VIDEO,   AV_CODEC_ID_H261, 90000, -1},
    {32, "MPV",        AVMEDIA_TYPE_VIDEO,   AV_CODEC_ID_MPEG1VIDEO, 90000, -1},
    {32, "MPV",        AVMEDIA_TYPE_VIDEO,   AV_CODEC_ID_MPEG2VIDEO, 90000, -1},
    {33, "MP2T",       AVMEDIA_TYPE_DATA,    AV_CODEC_ID_MPEG2TS, 90000, -1},
    {34, "H263",       AVMEDIA_TYPE_VIDEO,   AV_CODEC_ID_H263, 90000, -1},
    {96, "H264",       AVMEDIA_TYPE_VIDEO,   AV_CODEC_ID_H264, 90000, -1},
    {96, "MPEG4-GENERIC",  AVMEDIA_TYPE_VIDEO,   AV_CODEC_ID_MPEG4, 90000, -1},
    {96, "H265", AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_HEVC, 90000, -1}
}};

quint8 getPayloadType(AVCodecID codec, bool isVideo)
{
    // static payload type
    for (const auto& type: kStaticPayloadTypes)
    {
        if (type.codec_id == codec)
            return type.pt;
    }
    // dynamic payload type
    return isVideo ? kVideoPayloadType : kAudioPayloadType;
}

bool isCodecSupported(AVCodecID codecId)
{
    // TODO is it needed??
    switch (codecId)
    {
        case AV_CODEC_ID_NONE:
        case AV_CODEC_ID_H263:
        case AV_CODEC_ID_H263P:
        case AV_CODEC_ID_H264:
        case AV_CODEC_ID_MPEG1VIDEO:
        case AV_CODEC_ID_MPEG2VIDEO:
        case AV_CODEC_ID_MPEG4:
        case AV_CODEC_ID_AAC:
        case AV_CODEC_ID_MP2:
        case AV_CODEC_ID_MP3:
        case AV_CODEC_ID_PCM_ALAW:
        case AV_CODEC_ID_PCM_MULAW:
        case AV_CODEC_ID_PCM_S8:
        case AV_CODEC_ID_PCM_S16BE:
        case AV_CODEC_ID_PCM_S16LE:
        case AV_CODEC_ID_PCM_U16BE:
        case AV_CODEC_ID_PCM_U16LE:
        case AV_CODEC_ID_PCM_U8:
        case AV_CODEC_ID_MPEG2TS:
        case AV_CODEC_ID_AMR_NB:
        case AV_CODEC_ID_AMR_WB:
        case AV_CODEC_ID_VORBIS:
        case AV_CODEC_ID_THEORA:
        case AV_CODEC_ID_VP8:
        case AV_CODEC_ID_ADPCM_G722:
        case AV_CODEC_ID_ADPCM_G726:
        case AV_CODEC_ID_MJPEG:
            return true;
        default:
            return false;
    }
}

// Change payload type in SDP media attributes.
// #: "a=rtpmap:97 mpeg4-generic/32000/2", where 97 is payload type.
QString updatePayloadType(const QString& line, int payloadType)
{
    QString result;
    int index = line.indexOf(':');
    if (index < 0)
        return line;
    result.append(line.left(index + 1));
    result.append(QByteArray().setNum(payloadType));
    index = line.indexOf(' ');
    if (index > 0)
        result.append(line.mid(index));
    return result;
}

QList<QString> getSdpAttributesFromContext(
    AVCodecContext* codec, AVFormatContext* fmt, int payloadType)
{
    static const int kMaxSdpSize = 1024*16;
    char buffer[kMaxSdpSize];
    memset(buffer, 0, sizeof(buffer));

    AVFormatContext* contexts[1];
    AVStream* streams[1];
    AVFormatContext ctx;
    AVStream stream;
    memset(&stream, 0, sizeof(stream));
    memset(&ctx, 0, sizeof(ctx));
    if (fmt)
    {
        ctx.oformat = fmt->oformat;
        if (fmt->oformat)
            ctx.oformat->priv_class = fmt->oformat->priv_class;
    }
    ctx.nb_streams = 1;
    contexts[0] = &ctx;
    stream.codecpar = avcodec_parameters_alloc();
    avcodec_parameters_from_context(stream.codecpar, codec);
    streams[0] = &stream;
    ctx.streams = streams;
    av_sdp_create(contexts, 1, buffer, sizeof(buffer));
    QList<QString> lines = QString(buffer).split("\r\n");
    QList<QString> result;
    for (const auto& line: lines)
    {
        if (line.startsWith("a=rtpmap") ||
            line.startsWith("a=fmtp") ||
            line.startsWith("a=framesize"))
        {
            result.push_back(updatePayloadType(line, payloadType));
        }
    }
    avcodec_parameters_free(&stream.codecpar);
    return result;
}

QList<QString> getSdpAttributesFromMedia(QnConstAbstractMediaDataPtr media, int payloadType)
{
    AVCodecContext* codec = avcodec_alloc_context3(nullptr);
    codec->codec_type = AVMEDIA_TYPE_VIDEO;
    codec->codec_id = media->compressionType;
    if (media->context)
        QnFfmpegHelper::mediaContextToAvCodecContext(codec, media->context);

    // we need to build AVFormatContext due to ffmpeg bug sdp.c:424(not check oformat pointer)
    AVOutputFormat * oformat = av_guess_format("rtp", NULL, NULL);
    if (oformat == 0)
        return QList<QString>();
    AVFormatContext* fmt;
    if (avformat_alloc_output_context2(&fmt, oformat, 0, "") != 0)
        return QList<QString>();

    QList<QString> sdp = getSdpAttributesFromContext(codec, fmt, payloadType);
    avformat_close_input(&fmt);
    avcodec_close(codec);
    av_free(codec);
    return sdp;
}

QnUniversalRtpEncoder::QnUniversalRtpEncoder(const Config& config, nx::metrics::Storage* metrics):
    m_outputBuffer(CL_MEDIA_ALIGNMENT, 0),
    m_config(config),
    m_transcoder(config.transcoderConfig, metrics)
{
}

void QnUniversalRtpEncoder::buildSdp(
    QnConstAbstractMediaDataPtr mediaHigh,
    QnConstAbstractMediaDataPtr mediaLow,
    bool transcoding,
    MediaQuality quality)
{
    m_sdpAttributes.clear();
    if (transcoding || !m_isVideo || !m_config.useMultipleSdpPayloadTypes)
    {
        AVCodecContext* codec = m_isVideo
            ? m_transcoder.getVideoCodecContext()
            : m_transcoder.getAudioCodecContext();
        m_sdpAttributes =
            getSdpAttributesFromContext(codec, m_transcoder.getFormatContext(), m_payloadType);
    }
    else
    {
        int highPayloadType = m_payloadType;
        int lowPayloadType = kSecondaryStreamPayloadType;
        if (quality != MEDIA_Quality_High && quality != MEDIA_Quality_ForceHigh)
            std::swap(highPayloadType, lowPayloadType);

        if (mediaHigh)
            m_sdpAttributes.append(getSdpAttributesFromMedia(mediaHigh, highPayloadType));
        if (mediaLow)
            m_sdpAttributes.append(getSdpAttributesFromMedia(mediaLow, lowPayloadType));
        m_useSecondaryPayloadType = true;
    }
}

bool QnUniversalRtpEncoder::open(
    QnConstAbstractMediaDataPtr mediaHigh,
    QnConstAbstractMediaDataPtr mediaLow,
    MediaQuality requiredQuality,
    AVCodecID dstCodec,
    const QSize& videoSize,
    const QnLegacyTranscodingSettings& extraTranscodeParams)
{
    m_requiredQuality = requiredQuality;
    QnConstAbstractMediaDataPtr media =
        requiredQuality == MEDIA_Quality_High || requiredQuality == MEDIA_Quality_ForceHigh
        ? mediaHigh
        : mediaLow;

    if (!media)
        return false;

    bool transcodingEnabled = dstCodec != AV_CODEC_ID_NONE;
    m_isVideo = media->dataType == QnAbstractMediaData::VIDEO;
    m_codec = transcodingEnabled ? dstCodec : media->compressionType;
    m_payloadType = getPayloadType(m_codec, m_isVideo);
    QnTranscoder::TranscodeMethod method = transcodingEnabled
        ? QnTranscoder::TM_FfmpegTranscode
        : QnTranscoder::TM_DirectStreamCopy;

    if (m_transcoder.setContainer("rtp") != 0 || !isCodecSupported(dstCodec))
        return false;

    if (m_config.absoluteRtcpTimestamps)
    {
        // disable ffmpeg rtcp report in oder to build it manually
        m_transcoder.setFormatOption("rtpflags", "+skip_rtcp");
    }

    m_transcoder.setPacketizedMode(true);
    m_transcoder.setUseRealTimeOptimization(m_config.useRealTimeOptimization);
    int status = -1;
    if (media->dataType == QnAbstractMediaData::VIDEO)
    {
        m_transcoder.setTranscodingSettings(extraTranscodeParams);
        m_transcoder.setVideoCodec(m_codec, method, Qn::StreamQuality::normal, videoSize);
        status = m_transcoder.open(
            std::dynamic_pointer_cast<const QnCompressedVideoData>(media),
            QnConstCompressedAudioDataPtr());
    }
    else
    {
        m_transcoder.setAudioCodec(m_codec, method);
        status = m_transcoder.open(
            QnConstCompressedVideoDataPtr(),
            std::dynamic_pointer_cast<const QnCompressedAudioData>(media));
    }
    if (status != 0)
        return false;

    buildSdp(mediaHigh, mediaLow, transcodingEnabled, requiredQuality);
    return true;
}

QString QnUniversalRtpEncoder::getSdpMedia(bool isVideo, int trackId)
{
    static const QString kEndl = "\r\n";
    QString sdpMedia;
    QTextStream stream(&sdpMedia);
    stream << "m=" << (isVideo ? "video " : "audio ") << 0 << " RTP/AVP " << m_payloadType;
    if (m_useSecondaryPayloadType)
        stream << ' ' << kSecondaryStreamPayloadType;
    stream << kEndl;
    stream << "a=control:trackID=" << trackId << kEndl;
    stream << m_sdpAttributes.join(kEndl);
    stream << kEndl;
    return sdpMedia;
}

void QnUniversalRtpEncoder::setDataPacket(QnConstAbstractMediaDataPtr media)
{
    m_outputBuffer.clear();
    m_outputPos = 0;
    m_packetIndex = 0;
    m_isCurrentPacketSecondaryStream =
        m_useSecondaryPayloadType && media->isLQ() && m_requiredQuality != MEDIA_Quality_Low;
    m_transcoder.transcodePacket(media, &m_outputBuffer);
}

bool QnUniversalRtpEncoder::getNextPacket(QnByteArray& sendBuffer)
{
    const QVector<int> packets = m_transcoder.getPacketsSize();
    if (m_outputPos >= (int) m_outputBuffer.size() - nx::streaming::rtp::RtpHeader::kSize ||
        m_packetIndex >= packets.size())
        return false;

    auto timestamps = m_transcoder.getLastPacketTimestamp();
    nx::streaming::rtp::RtpHeader* rtpHeader =
        (nx::streaming::rtp::RtpHeader*)(m_outputBuffer.data() + m_outputPos);

    if (m_isCurrentPacketSecondaryStream)
        rtpHeader->payloadType = kSecondaryStreamPayloadType;

    if (m_config.absoluteRtcpTimestamps)
    {
        if (m_rtcpReporter.needReport(timestamps.ntpTimestamp, timestamps.rtpTimestamp))
        {
            uint8_t buffer[nx::streaming::rtp::RtcpSenderReport::kSize];
            int size = m_rtcpReporter.getReport().write(buffer, sizeof(buffer));
            sendBuffer.write((char*)buffer, size);
            return true;
        }

        // update rtp timestamp
        rtpHeader->timestamp = htonl(timestamps.rtpTimestamp);
        uint32_t payloadSize = packets[m_packetIndex] - nx::streaming::rtp::RtpHeader::kSize;
        m_rtcpReporter.onPacket(payloadSize);
    }

    const char* packetOffset = m_outputBuffer.data() + m_outputPos;
    uint32_t packetSize = packets[m_packetIndex];
    if (m_config.addOnvifHeaderExtension)
    {
        rtpHeader->extension = 1;
        sendBuffer.write(packetOffset, nx::streaming::rtp::RtpHeader::kSize);
        packetOffset += nx::streaming::rtp::RtpHeader::kSize;
        packetSize -= nx::streaming::rtp::RtpHeader::kSize;
        uint8_t buffer[nx::streaming::rtp::OnvifHeaderExtension::kSize];
        nx::streaming::rtp::OnvifHeaderExtension onvifExtension;
        onvifExtension.ntp = std::chrono::microseconds(timestamps.ntpTimestamp);
        uint32_t size = onvifExtension.write(buffer, sizeof(buffer));
        sendBuffer.write((char*)buffer, size);
    }
    sendBuffer.write(packetOffset, packetSize);
    m_outputPos += packets[m_packetIndex];
    m_packetIndex++;
    return true;
}

void QnUniversalRtpEncoder::init()
{
}

