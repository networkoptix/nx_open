#include "avcodec_container.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace webcam_plugin {

AVCodecContainer::~AVCodecContainer()
{
    close();
}

int AVCodecContainer::open()
{
    if (m_open)
        return 0;

    NX_DEBUG(this) << "open():: m_codecContext != nullptr:" << (m_codecContext != nullptr) <<
    ", m_codec != nullptr:" << (m_codec != nullptr);

    int codecOpenCode = avcodec_open2(m_codecContext, m_codec, nullptr);
    NX_DEBUG(this) << "open()::codecOpenCode:" << codecOpenCode;
    if (codecOpenCode < 0)
    {
        close();
        return codecOpenCode;
    }

    m_open = true;
    return codecOpenCode;
}

int AVCodecContainer::close()
{
    if (m_codecContext)
        avcodec_free_context(&m_codecContext);

    m_open = false;
    return 0;
}

int AVCodecContainer::readFrame(AVFormatContext * formatContext, AVPacket * outPacket)
{
    return av_read_frame(formatContext, outPacket);
}

int AVCodecContainer::encodeVideo(AVPacket * outPacket, const AVFrame * frame, int * outGotPacket) const
{
    return avcodec_encode_video2(m_codecContext, outPacket, frame, outGotPacket);
}


int AVCodecContainer::decodeVideo(AVFormatContext* formatContext, AVFrame * outFrame, int * outGotPicture, AVPacket * packet) const
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

int AVCodecContainer::encodeAudio(AVPacket * outPacket, const AVFrame * frame, int * outGotPacket) const
{
    return avcodec_encode_audio2(m_codecContext, outPacket, frame, outGotPacket);
}

int AVCodecContainer::decodeAudio(AVFrame * frame, int* outGotFrame, const AVPacket *packet) const
{
    return avcodec_decode_audio4(m_codecContext, frame, outGotFrame, packet);
}

int AVCodecContainer::initializeEncoder(AVCodecID codecID)
{
    m_codec = avcodec_find_encoder(codecID);
    if (!m_codec)
        return AVERROR_ENCODER_NOT_FOUND;

    m_codecContext = avcodec_alloc_context3(m_codec);
    if (!m_codecContext)
        return AVERROR(ENOMEM);

    return 0;
}

int AVCodecContainer::initializeDecoder(AVCodecParameters * codecParameters)
{
    m_codec = avcodec_find_decoder(codecParameters->codec_id);
    NX_DEBUG(this) << "initializeDecoder()::m_codec!=nullptr:" << (m_codec != nullptr);

    if (!m_codec)
        return AVERROR_DECODER_NOT_FOUND;

    m_codecContext = avcodec_alloc_context3(m_codec);
    NX_DEBUG(this) << "initializeDecoder()::m_codecContext != nullptr:" << (m_codecContext != nullptr);
    if(!m_codecContext)
        return AVERROR(ENOMEM);

    int paramToContextCode = avcodec_parameters_to_context(m_codecContext, codecParameters);
    NX_DEBUG(this) << "initializeDecoder()::paramToContextCode:"<< paramToContextCode;
    return paramToContextCode;
}

AVCodecContext * AVCodecContainer::codecContext() const
{
    return m_codecContext;
}

AVCodec * AVCodecContainer::codec() const
{
    return m_codec;
}

AVCodecID AVCodecContainer::codecID() const
{
    return m_codecContext->codec_id;
}

} // namespace webcam_plugin
} // namespace nx
