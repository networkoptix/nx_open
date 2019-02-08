#include "codec.h"

#include "utils.h"
#include "packet.h"
#include "frame.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/error.h>
}

namespace nx {
namespace usb_cam {
namespace ffmpeg {

Codec::~Codec()
{
    close();
}

int Codec::open()
{
    int result = avcodec_open2(m_codecContext, m_codec, &m_options);
    if (result < 0)
        close();
    return result;
}

void Codec::close()
{
    if (m_codecContext)
        avcodec_free_context(&m_codecContext);
}

int Codec::sendPacket(const AVPacket *packet) const
{
    return avcodec_send_packet(m_codecContext, packet);
}

int Codec::sendFrame(const AVFrame * frame) const
{
    return avcodec_send_frame(m_codecContext, frame);
}

int Codec::receivePacket(AVPacket * outPacket) const
{
    return avcodec_receive_packet(m_codecContext, outPacket);
}
    
int Codec::receiveFrame(AVFrame * outFrame) const
{
    return avcodec_receive_frame(m_codecContext, outFrame);
}

int Codec::initializeEncoder(AVCodecID codecId)
{
    close();
    m_codec = avcodec_find_encoder(codecId);
    if (!m_codec)
        return AVERROR_ENCODER_NOT_FOUND;

    m_codecContext = avcodec_alloc_context3(m_codec);
    if (!m_codecContext)
        return AVERROR(ENOMEM);

    return 0;
}

int Codec::initializeEncoder(const char * codecName)
{
    close();
    m_codec = avcodec_find_encoder_by_name(codecName);
    if (!m_codec)
        return AVERROR_ENCODER_NOT_FOUND;
        
    m_codecContext = avcodec_alloc_context3(m_codec);
    if (!m_codecContext)
        return AVERROR(ENOMEM);

    return 0;
}

int Codec::initializeDecoder(AVCodecID codecId)
{
    close();
    m_codec = avcodec_find_decoder(codecId);
    if (!m_codec)
        return AVERROR_DECODER_NOT_FOUND;

    m_codecContext = avcodec_alloc_context3(m_codec);
    if(!m_codecContext)
        return AVERROR(ENOMEM);

    return 0;
}

int Codec::initializeDecoder(const AVCodecParameters * codecParameters)
{
    int result = initializeDecoder(codecParameters->codec_id);
    if (result < 0)
        return result;
    
    result = avcodec_parameters_to_context(m_codecContext, codecParameters);
    if (result < 0)
        avcodec_free_context(&m_codecContext);
    
    return result;
}

int Codec::initializeDecoder(const char * codecName)
{
    close();
    m_codec = avcodec_find_decoder_by_name(codecName);
    if(!m_codec)
        return AVERROR_DECODER_NOT_FOUND;

    m_codecContext = avcodec_alloc_context3(m_codec);
    if(!m_codecContext)
        return AVERROR(ENOMEM);

    return 0;
}

void Codec::setFps(float fps)
{
    AVRational fraction = utils::toFraction(fps);
    m_codecContext->framerate = fraction;
    m_codecContext->time_base = {fraction.den, fraction.num};
}

void Codec::setResolution(int width, int height)
{
    m_codecContext->width = width;
    m_codecContext->height = height;
}

void Codec::setBitrate(int bitrate)
{
    m_codecContext->bit_rate = bitrate;
}

void Codec::setPixelFormat(AVPixelFormat pixelFormat)
{
    m_codecContext->pix_fmt = pixelFormat;
}

void Codec::resolution(int * outWidth, int * outHeight) const
{
    *outWidth = m_codecContext->width;
    *outHeight = m_codecContext->height;
}

const AVCodecContext * Codec::codecContext() const
{
    return m_codecContext;
}

AVCodecContext * Codec::codecContext()
{
    return m_codecContext;
}

const AVCodec * Codec::codec() const
{
    return m_codec;
}

AVCodecID Codec::codecId() const
{
    return m_codecContext->codec_id;
}

AVPixelFormat Codec::pixelFormat() const
{
    return m_codecContext->pix_fmt;
}

AVSampleFormat Codec::sampleFormat() const
{
    return m_codecContext->sample_fmt;
}

int Codec::sampleRate() const
{
    return m_codecContext->sample_rate;
}

int Codec::frameSize() const
{
    return m_codecContext->frame_size;
}

void Codec::flush()
{
    if (!avcodec_is_open(m_codecContext))
        return;

    if(av_codec_is_decoder(m_codec))
    {
        Frame frame;
        int returnCode = sendPacket(nullptr);
        while (returnCode != AVERROR_EOF)
            returnCode = receiveFrame(frame.frame());
    }
    else
    {
        Packet packet(AV_CODEC_ID_NONE, AVMEDIA_TYPE_UNKNOWN);
        int returnCode = sendFrame(nullptr);
        while (returnCode != AVERROR_EOF)
            returnCode = receivePacket(packet.packet());
    }
}

} // namespace ffmpeg
} // namespace usb_cam
} // namespace nx
