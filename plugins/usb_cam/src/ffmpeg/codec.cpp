#include "codec.h"

#include "utils.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/error.h>
}

namespace nx {
namespace ffmpeg {

Codec::Codec():
    Options()
{
    
}

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

int Codec::encode(AVPacket *outPacket, const AVFrame *frame, int *outGotPacket) const
{
    switch(m_codec->type)
    {
        case AVMEDIA_TYPE_VIDEO:
            return encodeVideo(outPacket, frame, outGotPacket);
        case AVMEDIA_TYPE_AUDIO:
            return encodeAudio(outPacket, frame, outGotPacket);
        default: // todo implement the rest of the encode functions
            return AVERROR_ENCODER_NOT_FOUND;
    }
}

int Codec::decode(AVFrame *outFrame, int *outGotFrame, const AVPacket *packet) const
{
    switch(m_codec->type)
    {
        case AVMEDIA_TYPE_VIDEO:
            return decodeVideo(outFrame, outGotFrame, packet);
        case AVMEDIA_TYPE_AUDIO:
            return decodeAudio(outFrame, outGotFrame, packet);
        default: // todo implement the rest of the decode functions
            return AVERROR_DECODER_NOT_FOUND;
    }
}

int Codec::encodeVideo(AVPacket * outPacket, const AVFrame * frame, int * outGotPacket) const
{
    return avcodec_encode_video2(m_codecContext, outPacket, frame, outGotPacket);
}

int Codec::decodeVideo(AVFrame * outFrame, int * outGotPicture, const AVPacket * packet) const
{
    return avcodec_decode_video2(m_codecContext, outFrame, outGotPicture, packet);
}

int Codec::encodeAudio(AVPacket * outPacket, const AVFrame * frame, int * outGotPacket) const
{
    return avcodec_encode_audio2(m_codecContext, outPacket, frame, outGotPacket);
}

int Codec::decodeAudio(AVFrame * outFrame, int* outGotFrame, const AVPacket *packet) const
{
    return avcodec_decode_audio4(m_codecContext, outFrame, outGotFrame, packet);
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

int Codec::initializeDecoder(AVCodecParameters * codecParameters)
{
    int initCode = initializeDecoder(codecParameters->codec_id);
    if(initCode < 0)
        return initCode;
    
    return avcodec_parameters_to_context(m_codecContext, codecParameters);
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
    //todo convert the float to a proper fraction
    m_codecContext->framerate = {(int)fps, 1};
    m_codecContext->time_base = {1, (int)fps};
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

} // namespace ffmpeg
} // namespace nx
