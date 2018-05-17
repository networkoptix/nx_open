#include "StdAfx.h"
#include "avcodec_container.h"

namespace nx {
namespace webcam_plugin {

AVCodecContainer::AVCodecContainer(AVFormatContext * formatContext)
    :
    m_formatContext(formatContext),
    m_codecContext(nullptr),
    m_codec(nullptr),
    m_open(false)
{
}

AVCodecContainer::~AVCodecContainer()
{
    close();
    if(m_codecContext)
        avcodec_free_context(&m_codecContext);
}

void AVCodecContainer::open()
{
    if (!isValid() || m_open)
        return;

    int codecOpenCode = avcodec_open2(m_codecContext, m_codec, nullptr);
    if (m_lastError.updateIfError(codecOpenCode))
    {
        //todo find out if i need to free the m_codecContext before returning
        return;
    }

    m_open = true;
}

void AVCodecContainer::close()
{
    if (!isValid() || !m_open)
        return;

    if (m_codecContext)
        avcodec_close(m_codecContext);
        
    m_open = false;
}

int AVCodecContainer::readFrame(AVPacket * outPacket)
{
    return av_read_frame(m_formatContext, outPacket);
}

int AVCodecContainer::encodeVideo(AVPacket * outPacket, const AVFrame * frame, int * outGotPacket)
{
    return avcodec_encode_video2(m_codecContext, outPacket, frame, outGotPacket);
}


int AVCodecContainer::decodeVideo(AVFrame * outFrame, int * outGotPicture, AVPacket * packet)
{
    int readCode;
    while ((readCode = av_read_frame(m_formatContext, packet) >= 0))
    {
        int decodeCode = avcodec_decode_video2(m_codecContext, outFrame, outGotPicture, packet);
        if (decodeCode < 0 || outGotPicture)
            return decodeCode;
    }
    return readCode;
}

int AVCodecContainer::encodeAudio(AVPacket * outPacket, const AVFrame * frame, int * outGotPacket)
{
    return avcodec_encode_audio2(m_codecContext, outPacket, frame, outGotPacket);
}

int AVCodecContainer::decodeAudio(AVFrame * frame, int* outGotFrame, const AVPacket *packet)
{
    return avcodec_decode_audio4(m_codecContext, frame, outGotFrame, packet);
}

void AVCodecContainer::initializeEncoder(AVCodecID codecID)
{
    m_codec = avcodec_find_encoder(codecID);
    if (!m_codec)
    {
        m_lastError.setAvError("couldn't find the encoder");
        return;
    }

    m_codecContext = avcodec_alloc_context3(m_codec);
    if (!m_codecContext)
    {
        m_lastError.setAvError("couldn't allocate codec context");
        return;
    }
}

void AVCodecContainer::initializeDecoder(AVCodecParameters * codecParameters)
{
    m_codec = avcodec_find_decoder(codecParameters->codec_id);
    if (!m_codec)
    {
        m_lastError.setAvError("couldn't find the decoder");
        return;
    }

    m_codecContext = avcodec_alloc_context3(m_codec);
    if (!m_codecContext)
    {
        m_lastError.setAvError("couldn't allocate codec context");
        return;
    }

    int paramToCodecContextCode = avcodec_parameters_to_context(m_codecContext, codecParameters);
    if (m_lastError.updateIfError(paramToCodecContextCode))
        return;
}

void AVCodecContainer::initializeDecoder(AVCodecID codecID)
{
    m_codec = avcodec_find_decoder(codecID);
    if (!m_codec)
    {
        m_lastError.setAvError("couldn't find the decoder");
        return;
    }

    m_codecContext = avcodec_alloc_context3(m_codec);
    if (!m_codecContext)
    {
        m_lastError.setAvError("couldn't allocate codec context");
        return;
    }

//    int paramToCodecContextCode = avcodec_parameters_to_context(m_codecContext, codecParameters);
  //  if (m_lastError.updateIfError(paramToCodecContextCode))
    //    return;
}

bool AVCodecContainer::isValid()
{
    return m_codecContext != nullptr
        && m_codec != nullptr
        && !m_lastError.hasError();
}


AVCodecContext * AVCodecContainer::codecContext()
{
    return m_codecContext;
}

AVCodecID AVCodecContainer::codecID()
{
    return m_codecContext->codec_id;
}

QString AVCodecContainer::avErrorString()
{
    return m_lastError.avErrorString();
}

} // namespace webcam_plugin
} // namespace nx
