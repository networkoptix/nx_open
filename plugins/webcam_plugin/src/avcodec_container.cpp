#include "StdAfx.h"
#include "avcodec_container.h"


AVCodecContainer::AVCodecContainer(AVFormatContext * formatContext, AVStream * stream, bool isEncoder):
    m_formatContext(formatContext),
    m_codecContext(nullptr),
    m_codec(nullptr),
    m_open(false)
{
    AVCodecID codecID = stream->codecpar->codec_id;
    m_codec = isEncoder ? avcodec_find_encoder(codecID) : avcodec_find_decoder(codecID);
    if (!m_codec)
    {
        QString codecType = isEncoder ? "encoder" : "decoder";
        m_lastError.setAvError(QString("couldnt find the %1").arg(codecType));
        return;
    }

    m_codecContext = avcodec_alloc_context3(nullptr);
    if (!m_codecContext)
    {
        m_lastError.setAvError("couldn't allocate codec context");
        return;
    }
    
    int paramToCodecContextCode = avcodec_parameters_to_context(m_codecContext, stream->codecpar);
    if (m_lastError.updateIfError(paramToCodecContextCode))
        return;

    int codecOpenCode = avcodec_open2(m_codecContext, m_codec, nullptr);
    if (m_lastError.updateIfError(codecOpenCode))
        return;

    m_open = true;
}

AVCodecContainer::AVCodecContainer(AVFormatContext * formatContext, AVCodecID codecID, bool isEncoder):
    m_formatContext(formatContext),
    m_codecContext(nullptr),
    m_codec(nullptr),
    m_open(false)
{
    initialize(codecID, isEncoder);
}

AVCodecContainer::~AVCodecContainer()
{
    close();
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
}

int AVCodecContainer::encode(AVPacket *outPacket, const AVFrame *frame, int *outGotPacket)
{
    return avcodec_encode_video2(m_codecContext, outPacket, frame, outGotPacket);
}

int AVCodecContainer::readAndDecode(AVFrame * outFrame, int * outGotPicture, AVPacket * packet, bool* outIsReadCode)
{
    if (!outFrame || !packet)
        return -1;

    int readCode;
    while ((readCode = av_read_frame(m_formatContext, packet) >= 0))
    {
        int decodeCode = avcodec_decode_video2(m_codecContext, outFrame, outGotPicture, packet);
        if (decodeCode < 0 || outGotPicture)
        {
            *outIsReadCode = false;
            return decodeCode;
        }
    }

    *outIsReadCode = true;
    return readCode;
}

bool AVCodecContainer::isValid()
{
    return m_codecContext != nullptr 
        && m_codec != nullptr 
        && !m_lastError.hasError();
}

AVCodecContext * AVCodecContainer::getCodecContext()
{
    return m_codecContext;
}

QString AVCodecContainer::getAvError()
{
    return m_lastError.getAvError();
}

void AVCodecContainer::initialize(AVCodecID codecID, bool isEncoder)
{
    m_codec = isEncoder ? avcodec_find_encoder(codecID) : avcodec_find_decoder(codecID);
    if (!m_codec)
    {
        QString codecType = isEncoder ? "encoder" : "decoder";
        m_lastError.setAvError(QString("couldnt find the %1").arg(codecType));
        return;
    }

    m_codecContext = avcodec_alloc_context3(nullptr);
    if (!m_codecContext)
    {
        m_lastError.setAvError("couldn't allocate codec context");
        return;
    }

    //open();
}
