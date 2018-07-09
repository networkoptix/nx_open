#include "rtp_universal_encoder.h"
#include <nx/streaming/rtp_stream_parser.h>
#include <nx/streaming/config.h>
#include "common/common_module.h"

#include <core/resource/param.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

static const int  RTP_PT_PRIVATE = 96;

/* from http://www.iana.org/assignments/rtp-parameters last updated 05 January 2005 */
/* payload types >= 96 are dynamic;
 * payload types between 72 and 76 are reserved for RTCP conflict avoidance;
 * all the other payload types not present in the table are unassigned or
 * reserved
 */
static const struct
{
    int pt;
    const char enc_name[14];
    enum AVMediaType codec_type;
    enum AVCodecID codec_id;
    int clock_rate;
    int audio_channels;
} AVRtpPayloadTypes[]=
{
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
  {97, "mpa-robust",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_MP3, -1, -1},
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
  {96, "H265", AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_HEVC, 90000, -1},
  {-1, "",           AVMEDIA_TYPE_UNKNOWN, AV_CODEC_ID_NONE, -1, -1}
};

QnUniversalRtpEncoder::QnUniversalRtpEncoder(
    QnCommonModule* commonModule,
    QnConstAbstractMediaDataPtr media,
    AVCodecID transcodeToCodec,
    const QSize& videoSize,
    const QnLegacyTranscodingSettings& extraTranscodeParams)
:
    m_outputBuffer(CL_MEDIA_ALIGNMENT, 0),
    m_outputPos(0),
    packetIndex(0),
    //m_firstTime(0),
    //m_isFirstPacket(true),
    m_isOpened(false)
{
    if( m_transcoder.setContainer("rtp") != 0 )
        return; //m_isOpened = false
    m_transcoder.setPacketizedMode(true);
    m_codec = transcodeToCodec != AV_CODEC_ID_NONE ? transcodeToCodec : media->compressionType;
    QnTranscoder::TranscodeMethod method;
    m_isVideo = media->dataType == QnAbstractMediaData::VIDEO;
    if (m_isVideo)
        method = transcodeToCodec != AV_CODEC_ID_NONE ? QnTranscoder::TM_FfmpegTranscode : QnTranscoder::TM_DirectStreamCopy;
    else
        method = media->compressionType == transcodeToCodec ? QnTranscoder::TM_DirectStreamCopy : QnTranscoder::TM_FfmpegTranscode;

    if (media->dataType == QnAbstractMediaData::VIDEO) {
        m_transcoder.setTranscodingSettings(extraTranscodeParams);
        m_transcoder.setVideoCodec(m_codec, method, Qn::StreamQuality::normal, videoSize);
    }
    else {
        m_transcoder.setAudioCodec(m_codec, method);
    }

    if (commonModule->isTranscodeDisabled() && method != QnTranscoder::TM_DirectStreamCopy)
    {
        m_isOpened = false;
        qWarning() << "Video transcoding is disabled in the server settings. Feature unavailable.";
    }
    else if (m_isVideo)
    {
        m_isOpened = m_transcoder.open(std::dynamic_pointer_cast<const QnCompressedVideoData>(media), QnConstCompressedAudioDataPtr()) == 0;
    }
    else
    {
        m_isOpened = m_transcoder.open(QnConstCompressedVideoDataPtr(), std::dynamic_pointer_cast<const QnCompressedAudioData>(media)) == 0;
    }
}

QByteArray updatePayloadType(const QByteArray& line, int payloadType){
    QByteArray result;
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

QByteArray QnUniversalRtpEncoder::getAdditionSDP(const std::map<QString, QString>& streamParams)
{
    AVCodecContext* codec =
        m_isVideo ? m_transcoder.getVideoCodecContext() : m_transcoder.getAudioCodecContext();
    AVFormatContext* fmt = m_transcoder.getFormatContext();
    int payloadType = getPayloadtype();
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
    QList<QByteArray> lines = QByteArray(buffer).split('\n');
    QByteArray result;
    for (const auto& line: lines)
    {
        if (line.startsWith("a=rtpmap") || line.startsWith("a=fmtp"))
            result.append(updatePayloadType(line, payloadType));
    }
    avcodec_parameters_free(&stream.codecpar);
    return result;
}

void QnUniversalRtpEncoder::setDataPacket(QnConstAbstractMediaDataPtr media)
{
    m_outputBuffer.clear();
    m_outputPos = 0;
    packetIndex = 0;
    m_transcoder.transcodePacket(media, &m_outputBuffer);
}

bool QnUniversalRtpEncoder::getNextPacket(QnByteArray& sendBuffer)
{
    const QVector<int> packets = m_transcoder.getPacketsSize();
    if (m_outputPos >= (int) m_outputBuffer.size() - RtpHeader::RTP_HEADER_SIZE || packetIndex >= packets.size())
        return false;

    /*
    if (packets[packetIndex] >= 12) {
        quint32* srcBuffer = (quint32*) (m_outputBuffer.data() + m_outputPos);
        RtpHeader* rtpHeader = (RtpHeader*) srcBuffer;
        bool isRtcp = rtpHeader->payloadType >= 72 && rtpHeader->payloadType <= 76;
        if (!isRtcp)
            rtpHeader->ssrc = htonl(getSSRC());
        else
            srcBuffer[1] = htonl(getSSRC());
    }
    */

    sendBuffer.write(m_outputBuffer.data() + m_outputPos, packets[packetIndex]);
    m_outputPos += packets[packetIndex];
    packetIndex++;
    return true;
}

void QnUniversalRtpEncoder::init()
{
}

quint32 QnUniversalRtpEncoder::getSSRC()
{
    return 15000 + (m_isVideo ? 0 : 1);
}

bool QnUniversalRtpEncoder::getRtpMarker()
{
    // unused
    return false;
}

quint32 QnUniversalRtpEncoder::getFrequency()
{
    for (int i = 0; AVRtpPayloadTypes[i].pt >= 0; ++i)
    {
        if (AVRtpPayloadTypes[i].codec_id == m_codec && AVRtpPayloadTypes[i].clock_rate != -1)
            return AVRtpPayloadTypes[i].clock_rate;
    }
    if (m_transcoder.getFormatContext())
    {
        //AVCodecContext* codecCtx =  m_transcoder.getFormatContext()->streams[0]->codec;
        //if (codecCtx)
        //    return codecCtx->time_base.den / codecCtx->time_base.num;
        AVStream* stream = m_transcoder.getFormatContext()->streams[0];
        return stream->time_base.den / stream->time_base.num;
    }

    return 90000;
}

quint8 QnUniversalRtpEncoder::getPayloadtype()
{
    // static payload type
    for (int i = 0; AVRtpPayloadTypes[i].pt >= 0; ++i)
    {
        if (AVRtpPayloadTypes[i].codec_id == m_codec)
            return AVRtpPayloadTypes[i].pt;
    }
    // dynamic payload type
    return RTP_PT_PRIVATE + (m_isVideo ? 0 : 1);
}

QString QnUniversalRtpEncoder::getName()
{
    for (int i = 0; AVRtpPayloadTypes[i].pt >= 0; ++i)
    {
        if (AVRtpPayloadTypes[i].codec_id == m_codec)
            return AVRtpPayloadTypes[i].enc_name;
    }
    return QString();
}

bool QnUniversalRtpEncoder::isOpened() const
{
    return m_isOpened;
}

void QnUniversalRtpEncoder::setUseRealTimeOptimization(bool value)
{
    m_transcoder.setUseRealTimeOptimization(value);
}
