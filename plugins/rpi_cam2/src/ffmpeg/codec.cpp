#include "codec.h"

#include <nx/utils/log/log.h>

#include "ffmpeg/utils.h"

namespace nx {
namespace webcam_plugin {
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

int Codec::readFrame(AVFormatContext * formatContext, AVPacket * outPacket)
{
    return av_read_frame(formatContext, outPacket);
}

int Codec::encodeVideo(AVPacket * outPacket, const AVFrame * frame, int * outGotPacket) const
{
    return avcodec_encode_video2(m_codecContext, outPacket, frame, outGotPacket);
}


int Codec::decodeVideo(AVFormatContext* formatContext, AVFrame * outFrame, int * outGotPicture, AVPacket * packet) const
{
    int readCode;
    while ((readCode = av_read_frame(formatContext, packet) >= 0))
    {
        int decodeCode = avcodec_decode_video2(m_codecContext, outFrame, outGotPicture, packet);
        if (decodeCode < 0 || outGotPicture)
            return decodeCode;
    }
    return readCode;
}

int Codec::encodeAudio(AVPacket * outPacket, const AVFrame * frame, int * outGotPacket) const
{
    return avcodec_encode_audio2(m_codecContext, outPacket, frame, outGotPacket);
}

int Codec::decodeAudio(AVFormatContext* formatContext, AVFrame * outFrame, int* outGotFrame, AVPacket *packet) const
{
    int readCode;
    while ((readCode = av_read_frame(formatContext, packet) >= 0))
    {
        int decodeCode = avcodec_decode_audio4(m_codecContext, outFrame, outGotFrame, packet);
        if (decodeCode < 0 || outGotFrame)
            return decodeCode;
    }
    return readCode;
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
    m_codec = avcodec_find_decoder(codecParameters->codec_id);
    if (!m_codec)
        return AVERROR_ENCODER_NOT_FOUND;

    m_codecContext = avcodec_alloc_context3(m_codec);
    if(!m_codecContext)
        return AVERROR(ENOMEM);

    int paramToContextCode = avcodec_parameters_to_context(m_codecContext, codecParameters);
    return paramToContextCode;
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

void Codec::setFps(int fps)
{
    m_codecContext->time_base = {1, fps};
    m_codecContext->framerate = {fps, 1};
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

} // namespace ffmpeg
} // namespace webcam_plugin
} // namespace nx
