// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rtp_universal_encoder.h"

#include <array>

#include <core/resource/resource_property_key.h>
#include <media/filters/remove_aud_delimiter.h>
#include <nx/media/config.h>
#include <nx/media/utils.h>
#include <nx/rtp/onvif_header_extension.h>
#include <nx/utils/log/log_main.h>
#include <transcoding/transcoding_utils.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

using namespace nx::rtsp;

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

QSize getVideoSize(QnConstAbstractMediaDataPtr media)
{
    QnConstCompressedVideoDataPtr videoData =
        std::dynamic_pointer_cast<const QnCompressedVideoData>(media);
    if (!videoData)
    {
        NX_WARNING(NX_SCOPE_TAG, "Invalid media data");
        return QSize();
    }
    return nx::media::getFrameSize(videoData.get());
}

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
        case AV_CODEC_ID_H265:
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

void fixProfileConstraints(QString& properties)
{
    // Check if base or main profile and no constraint flags -> set them(for Safari WebRTC
    // playback).
    // #: Convert string like: profile-level-id=420033 to profile-level-id=42C033.
    const QString profile = "profile-level-id=";
    auto index = properties.indexOf(profile);
    if (index != -1)
    {
        index = index + profile.length();
        if (properties.size() < index + 3)
            return;

        bool status = false;
        int idc = properties.mid(index, 2).toInt(&status, 16);
        if (!status)
            return;

        if (idc == 66 || idc == 77) //< Base or main profile.
            properties[index + 2] = QChar('C');
    }
}

QList<QString> getSdpAttributesFromContext(AVFormatContext* fmt, int payloadType, bool webRtcMode)
{
    AVCodecParameters* codecpar = nullptr;
    if (fmt && fmt->streams[0] && fmt->streams[0]->codecpar)
        codecpar = fmt->streams[0]->codecpar;

    static const int kMaxSdpSize = 1024*16;
    char buffer[kMaxSdpSize];
    memset(buffer, 0, sizeof(buffer));

    av_sdp_create(&fmt, 1, buffer, sizeof(buffer));
    QList<QString> lines = QString(buffer).split("\r\n");
    QList<QString> result;
    for (auto& line: lines)
    {
        if (line.startsWith("a=rtpmap") ||
            line.startsWith("a=fmtp") ||
            line.startsWith("a=framesize"))
        {
            bool isH264 = codecpar && codecpar->codec_id == AV_CODEC_ID_H264;
            if (webRtcMode && isH264 && line.startsWith("a=fmtp"))
            {
                fixProfileConstraints(line);
                line.append("; level-asymmetry-allowed=1");
            }

            result.push_back(updatePayloadType(line, payloadType));
        }
    }
    if (codecpar && codecpar->codec_id == AV_CODEC_ID_MJPEG)
    {
        result.push_back(nx::format("a=framesize:%1 %2-%3").args(
            payloadType, codecpar->width, codecpar->height));
    }
    return result;
}

QList<QString> getSdpAttributesFromCodecpar(
    const AVCodecParameters* codecParameters, int payloadType, bool webRtcMode)
{
    if (!codecParameters)
        return QList<QString>();

    // We need to build AVFormatContext due to ffmpeg bug sdp.c:526(not checking oformat pointer).
    const AVOutputFormat* oformat = av_guess_format("rtp", NULL, NULL);
    if (oformat == 0)
        return QList<QString>();
    AVFormatContext* fmt;
    if (avformat_alloc_output_context2(&fmt, oformat, 0, "") != 0)
        return QList<QString>();

    AVStream* streams[1];
    AVFormatContext ctx;
    AVStream stream;
    memset(&stream, 0, sizeof(stream));
    memset(&ctx, 0, sizeof(ctx));
    ctx.nb_streams = 1;
    stream.codecpar = avcodec_parameters_alloc();
    avcodec_parameters_copy(stream.codecpar, codecParameters);
    streams[0] = &stream;
    ctx.streams = streams;
    QList<QString> sdp = getSdpAttributesFromContext(&ctx, payloadType, webRtcMode);
    avcodec_parameters_free(&stream.codecpar);
    avformat_free_context(fmt);
    return sdp;
}

QnUniversalRtpEncoder::QnUniversalRtpEncoder(const Config& config, nx::metric::Storage* metrics):
    m_outputBuffer(CL_MEDIA_ALIGNMENT, 0, AV_INPUT_BUFFER_PADDING_SIZE),
    m_config(config),
    m_transcoder(config.transcoderConfig, metrics)
{
    if (m_config.useRtcpNack)
        m_rtcpNackResponder = std::make_unique<nx::rtp::RtcpNackResponder>();
    if (m_config.webRtcMode)
        m_transcoder.muxer().setVideoStreamFilter(std::make_unique<H2645RemoveAudDelimiter>());
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
        m_sdpAttributes = getSdpAttributesFromContext(
            m_transcoder.muxer().getFormatContext(), m_payloadType, m_config.webRtcMode);
    }
    else
    {
        int highPayloadType = m_payloadType;
        int lowPayloadType = kSecondaryStreamPayloadType;
        if (quality != MEDIA_Quality_High && quality != MEDIA_Quality_ForceHigh)
            std::swap(highPayloadType, lowPayloadType);

        if (mediaHigh && mediaHigh->context)
        {
            m_sdpAttributes.append(getSdpAttributesFromCodecpar(
                    mediaHigh->context->getAvCodecParameters(),
                    highPayloadType,
                    m_config.webRtcMode));
        }
        if (mediaLow && mediaLow->context)
        {
            m_sdpAttributes.append(getSdpAttributesFromCodecpar(
                mediaLow->context->getAvCodecParameters(),
                lowPayloadType,
                m_config.webRtcMode));
        }
        m_useSecondaryPayloadType = true;
    }
}

bool QnUniversalRtpEncoder::open(
    QnConstAbstractMediaDataPtr mediaHigh,
    QnConstAbstractMediaDataPtr mediaLow,
    MediaQuality requiredQuality,
    AVCodecID dstCodec,
    const QSize& videoSize,
    const nx::core::transcoding::Settings& extraTranscodeParams)
{
    m_requiredQuality = requiredQuality;
    QnConstAbstractMediaDataPtr media =
        requiredQuality == MEDIA_Quality_High || requiredQuality == MEDIA_Quality_ForceHigh
        ? mediaHigh
        : mediaLow;

    if (!media)
        return false;

    m_transcodingEnabled = dstCodec != AV_CODEC_ID_NONE;
    m_isVideo = media->dataType == QnAbstractMediaData::VIDEO;
    m_sourceCodec = media->compressionType;
    AVCodecID codec = m_transcodingEnabled ? dstCodec : media->compressionType;
    m_payloadType = getPayloadType(codec, m_isVideo);
    QnFfmpegTranscoder::TranscodeMethod method = m_transcodingEnabled
        ? QnFfmpegTranscoder::TM_FfmpegTranscode
        : QnFfmpegTranscoder::TM_DirectStreamCopy;

    if (!m_transcoder.muxer().setContainer("rtp") || !isCodecSupported(dstCodec))
        return false;

    if (m_config.absoluteRtcpTimestamps)
    {
        // disable ffmpeg rtcp report in oder to build it manually
        m_transcoder.muxer().setFormatOption("rtpflags", "+skip_rtcp");
    }

    m_transcoder.muxer().setPacketizedMode(true);
    bool status = false;
    if (media->dataType == QnAbstractMediaData::VIDEO)
    {
        QSize sourceSize;
        if (method == QnFfmpegTranscoder::TM_FfmpegTranscode)
        {
            sourceSize = getVideoSize(media);
            if (sourceSize.isEmpty())
                NX_DEBUG(this, "Failed to get frame size, codec: %1", media->compressionType);
        }
        m_transcoder.setTranscodingSettings(extraTranscodeParams);
        m_transcoder.setVideoCodec(codec, method, Qn::StreamQuality::normal, sourceSize, videoSize);
        status = m_transcoder.open(
            std::dynamic_pointer_cast<const QnCompressedVideoData>(media),
            QnConstCompressedAudioDataPtr());
    }
    else
    {
        m_transcoder.setAudioCodec(codec, method);
        status = m_transcoder.open(
            QnConstCompressedVideoDataPtr(),
            std::dynamic_pointer_cast<const QnCompressedAudioData>(media));
    }
    if (!status)
        return false;

    buildSdp(mediaHigh, mediaLow, m_transcodingEnabled, requiredQuality);
    return true;
}

bool QnUniversalRtpEncoder::open(const AVCodecParameters* codecParameters)
{
    if (!codecParameters)
    {
        NX_WARNING(this, "Empty codec parameters");
        return false;
    }
    m_isVideo = codecParameters->codec_type == AVMEDIA_TYPE_VIDEO;
    m_sourceCodec = codecParameters->codec_id;

    if (!m_transcoder.muxer().setContainer("rtp") || !isCodecSupported(codecParameters->codec_id))
        return false;

    if (m_config.absoluteRtcpTimestamps)
    {
        // disable ffmpeg rtcp report in oder to build it manually
        m_transcoder.muxer().setFormatOption("rtpflags", "+skip_rtcp");
    }

    m_transcoder.muxer().setPacketizedMode(true);
    if (!m_transcoder.open(codecParameters))
    {
        NX_WARNING(this, "Failed to open Ffmpeg muxer");
        return false;
    }
    m_payloadType = getPayloadType(codecParameters->codec_id, m_isVideo);
    m_sdpAttributes = getSdpAttributesFromCodecpar(
        codecParameters, m_payloadType, m_config.webRtcMode);
    return true;
}

void QnUniversalRtpEncoder::setMtu(int mtu)
{
    m_transcoder.muxer().setRtpMtu(mtu);
}

QString QnUniversalRtpEncoder::getSdpMedia(bool isVideo, int trackId, int port, bool ssl)
{
    static const QString kEndl = "\r\n";
    QString sdpMedia;
    QTextStream stream(&sdpMedia);
    stream << "m=" << (isVideo ? "video " : "audio ") << port <<
        (ssl ? " UDP/TLS/RTP/SAVPF " : " RTP/AVP ") << m_payloadType;
    if (m_useSecondaryPayloadType)
        stream << ' ' << kSecondaryStreamPayloadType;
    stream << kEndl;
    stream << "a=control:trackID=" << trackId << kEndl;
    stream << m_sdpAttributes.join(kEndl);
    if (!m_sdpAttributes.empty())
        stream << kEndl;

    return sdpMedia;
}

void QnUniversalRtpEncoder::setRtcpPacket(uint8_t* data, int size)
{
    using namespace nx::rtp;
    if (size < kRtcpHeaderSize || data == nullptr)
        return;
    uint8_t messageType = data[1];
    NX_VERBOSE(this, "Got RTCP Packet with size %1 and type %2", size, messageType);
    switch (messageType)
    {
        case kRtcpSenderReport:
        case kRtcpReceiverReport:
        case kRtcpSourceDesciption:
        case kRtcpPayloadSpecificFeedback:
            break;
        case kRtcpGenericFeedback:
            if (m_config.useRtcpNack && m_rtcpNackResponder)
                m_rtcpNackResponder->pushRtcpNack(data, size);
            break;
        default:
            break;
    }
}

bool QnUniversalRtpEncoder::getNextNackPacket(nx::utils::ByteArray& sendBuffer)
{
    return m_rtcpNackResponder && m_rtcpNackResponder->getNextPacket(sendBuffer);
}

void QnUniversalRtpEncoder::setDataPacket(QnConstAbstractMediaDataPtr media)
{
    // Do not continue rtsp stream if codec changed.
    if (!m_transcodingEnabled && media && m_sourceCodec != media->compressionType)
    {
        NX_DEBUG(this, "Codec changed from %1 to %2, close rtsp session",
            m_sourceCodec, media->compressionType);
        m_eof = true;
    }
    if (m_eof)
        return;

    m_outputBuffer.clear();
    m_outputPos = 0;
    m_packetIndex = 0;
    m_isCurrentPacketSecondaryStream =
        m_useSecondaryPayloadType && media->isLQ() && m_requiredQuality != MEDIA_Quality_Low;
    m_transcoder.transcodePacket(media, &m_outputBuffer);
}

bool QnUniversalRtpEncoder::getNextPacket(nx::utils::ByteArray& sendBuffer)
{
    const int dataStartIndex = sendBuffer.size();

    if (m_eof)
        return false;

    const QVector<int> packets = m_transcoder.muxer().getPacketsSize();
    if (m_outputPos >= (int) m_outputBuffer.size() - nx::rtp::RtpHeader::kSize ||
        m_packetIndex >= packets.size())
        return false;

    auto timestamps = m_transcoder.muxer().getLastPacketTimestamp();
    nx::rtp::RtpHeader* rtpHeader =
        (nx::rtp::RtpHeader*)(m_outputBuffer.data() + m_outputPos);

    if (m_isCurrentPacketSecondaryStream)
        rtpHeader->payloadType = kSecondaryStreamPayloadType;
    if (m_ssrc)
        rtpHeader->ssrc = qToBigEndian(m_ssrc.value());

    if (m_config.absoluteRtcpTimestamps)
    {
        if (m_rtcpReporter.needReport(timestamps.ntpTimestamp, timestamps.rtpTimestamp))
        {
            using namespace nx::rtp;
            uint8_t buffer[256];
            if (m_ssrc)
                m_rtcpReporter.setSsrc(*m_ssrc);
            if (m_cname)
                m_rtcpReporter.setCName(*m_cname);
            int size = m_rtcpReporter.getReport().write(buffer, sizeof(buffer));

            sendBuffer.write((char*)buffer, size);
            if (m_encryptor)
                return m_encryptor->encryptPacket(&sendBuffer, dataStartIndex);
            return true;
        }

        // update rtp timestamp
        rtpHeader->timestamp = htonl(timestamps.rtpTimestamp);
        uint32_t payloadSize = packets[m_packetIndex] - nx::rtp::RtpHeader::kSize;
        m_rtcpReporter.onPacket(payloadSize);
    }

    const char* packetOffset = m_outputBuffer.data() + m_outputPos;
    uint32_t packetSize = packets[m_packetIndex];
    if (m_config.addOnvifHeaderExtension)
    {
        rtpHeader->extension = 1;
        sendBuffer.write(packetOffset, nx::rtp::RtpHeader::kSize);
        packetOffset += nx::rtp::RtpHeader::kSize;
        packetSize -= nx::rtp::RtpHeader::kSize;
        uint8_t buffer[nx::rtp::OnvifHeaderExtension::kSize];
        nx::rtp::OnvifHeaderExtension onvifExtension;
        onvifExtension.ntp = std::chrono::microseconds(timestamps.ntpTimestamp);
        uint32_t size = onvifExtension.write(buffer, sizeof(buffer));
        sendBuffer.write((char*)buffer, size);
    }
    sendBuffer.write(packetOffset, packetSize);
    m_outputPos += packets[m_packetIndex];
    m_packetIndex++;

    if (m_encryptor && !m_encryptor->encryptPacket(&sendBuffer, dataStartIndex))
        return false;

    if (m_config.useRtcpNack && m_rtcpNackResponder)
        m_rtcpNackResponder->pushPacket(sendBuffer, rtpHeader->getSequence());

    return true;
}

void QnUniversalRtpEncoder::init()
{
}

bool QnUniversalRtpEncoder::isEof() const
{
    return m_eof;
}

void QnUniversalRtpEncoder::setSrtpEncryptionData(const EncryptionData& data)
{
    m_encryptor = std::make_unique<SrtpEncryptor>();
    if (!m_encryptor->init(data))
    {
        NX_WARNING(this, "Failure to init SRTP encryptor");
        m_encryptor.reset();
    }
}

FfmpegMuxer::PacketTimestamp QnUniversalRtpEncoder::getLastTimestamps() const
{
    return m_transcoder.muxer().getLastPacketTimestamp();
}

nx::rtsp::SrtpEncryptor* QnUniversalRtpEncoder::encryptor() const
{
    return m_encryptor.get();
}

int QnUniversalRtpEncoder::payloadType() const
{
    return m_payloadType;
}
