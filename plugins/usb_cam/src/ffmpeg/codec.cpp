#include "codec.h"

#include "utils.h"
#include "packet.h"
#include "frame.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/error.h>
}

namespace nx {
namespace ffmpeg {

Codec::~Codec()
{
    close();
}

int Codec::open()
{
    int codecOpenCode = avcodec_open2(m_codecContext, m_codec, &m_options);
    if (codecOpenCode < 0)
    {
        close();
        return codecOpenCode;
    }

    return codecOpenCode;
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

int Codec::decode(AVFrame * outFrame, const AVPacket * packet, int * outGotFrame)
{
    switch (m_codec->type)
    {
        case AVMEDIA_TYPE_VIDEO: 
            return decodeVideo(outFrame, packet, outGotFrame);
        case AVMEDIA_TYPE_AUDIO: 
            return decodeAudio(outFrame, packet, outGotFrame);
        default: 
            return AVERROR_DECODER_NOT_FOUND;
    }
}

int Codec::encode(const AVFrame * frame, AVPacket * outPacket, int * outGotPacket)
{
    switch (m_codec->type)
    {
        case AVMEDIA_TYPE_VIDEO: 
            return encodeVideo(frame, outPacket, outGotPacket);
        case AVMEDIA_TYPE_AUDIO:
             return encodeAudio(frame, outPacket, outGotPacket);
        default: 
            return AVERROR_ENCODER_NOT_FOUND;
    }
}

int Codec::decodeVideo(AVFrame * outFrame, const AVPacket * packet, int * outGotFrame)
{
    return avcodec_decode_video2(m_codecContext, outFrame, outGotFrame, packet);
}

int Codec::encodeVideo(const AVFrame * frame, AVPacket * outPacket, int * outGotPacket)
{
    return avcodec_encode_video2(m_codecContext, outPacket, frame, outGotPacket);
}

int Codec::decodeAudio(AVFrame * outFrame, const AVPacket * packet, int * outGotFrame)
{
    return avcodec_decode_audio4(m_codecContext, outFrame, outGotFrame, packet);
}

int Codec::encodeAudio(const AVFrame * frame, AVPacket * outPacket, int * outGotPacket)
{
    return avcodec_encode_audio2(m_codecContext, outPacket, frame, outGotPacket);
}

int Codec::initializeEncoder(AVCodecID codecID)
{
    m_codec = avcodec_find_encoder(codecID);
    if (!m_codec)
        return AVERROR_ENCODER_NOT_FOUND;

    m_codecContext = avcodec_alloc_context3(m_codec);
    if (!m_codecContext)
        return AVERROR(ENOMEM);

    return 0;
}

int Codec::initializeEncoder(const char * codecName)
{
    m_codec = avcodec_find_encoder_by_name(codecName);
    if (!m_codec)
        return AVERROR_ENCODER_NOT_FOUND;
        
    m_codecContext = avcodec_alloc_context3(m_codec);
    if (!m_codecContext)
        return AVERROR(ENOMEM);

    return 0;
}

int Codec::initializeDecoder(AVCodecID codecID)
{
    m_codec = avcodec_find_decoder(codecID);
    if (!m_codec)
        return AVERROR_ENCODER_NOT_FOUND;

    m_codecContext = avcodec_alloc_context3(m_codec);
    if(!m_codecContext)
        return AVERROR(ENOMEM);

    return 0;
}

int Codec::initializeDecoder(const AVCodecParameters * codecParameters)
{
    int initCode = initializeDecoder(codecParameters->codec_id);
    if(initCode < 0)
        return initCode;
    
    return avcodec_parameters_to_context(m_codecContext, codecParameters);
}

int Codec::initializeDecoder(const char * codecName)
{
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
    int numerator = 0;
    int denominator = 0;
    utils::toFraction(fps, &numerator, &denominator);

    m_codecContext->framerate = {numerator, denominator};
    m_codecContext->time_base = {denominator, numerator};
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

AVCodecContext * Codec::codecContext() const
{
    return m_codecContext;
}

AVCodec * Codec::codec() const
{
    return m_codec;
}

AVCodecID Codec::codecID() const
{
    return m_codecContext->codec_id;
}

AVPixelFormat Codec::pixelFormat() const
{
    return m_codecContext->pix_fmt;
}

void Codec::flush() const
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
        Packet packet(AV_CODEC_ID_NONE);
        int returnCode = sendFrame(nullptr);
        while (returnCode != AVERROR_EOF)
            returnCode = receivePacket(packet.packet());
    }
}

} // namespace ffmpeg
} // namespace nx
