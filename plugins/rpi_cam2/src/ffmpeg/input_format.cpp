#include "input_format.h"

#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

#include "error.h"

namespace nx {
namespace ffmpeg {

InputFormat::InputFormat():
    Options(),
    m_gopSize(0),
    m_gopSearch(0)
{    
}

InputFormat::~InputFormat()
{
    close();
}

int InputFormat::initialize(const char * deviceType)
{
    m_inputFormat = av_find_input_format(deviceType); 
    if (!m_inputFormat)
    {
        // there is no error code for format not found
        error::setLastError(AVERROR_PROTOCOL_NOT_FOUND);
        return AVERROR_PROTOCOL_NOT_FOUND;
    }

    m_formatContext = avformat_alloc_context();
    if (!m_formatContext)
    {
        error::setLastError(AVERROR(ENOMEM));
        return AVERROR(ENOMEM);
    }

    return 0;
}

int InputFormat::open(const char * url)
{
    int openCode = avformat_open_input(
        &m_formatContext,
        url,
        m_inputFormat,
        &m_options);
    error::updateIfError(openCode);
    return openCode;
}

void InputFormat::close()
{
    if (m_formatContext)
        avformat_close_input(&m_formatContext);
}

int InputFormat::readFrame(AVPacket * outPacket)
{
    int readCode = av_read_frame(m_formatContext, outPacket);
    if(!error::updateIfError(readCode) && !m_gopSize)
        calculateGopSize(outPacket);
    return readCode;
}

int InputFormat::setFps(int fps)
{
    return setEntry("framerate", std::to_string(fps).c_str());
}

int InputFormat::setResolution(int width, int height)
{
    return setEntry("video_size", (std::to_string(width) + "x" + std::to_string(height)).c_str());
}

AVCodecID InputFormat::videoCodecID() const
{
    return m_formatContext->video_codec_id;
}

AVCodecID InputFormat::audioCodecID() const
{
    return m_formatContext->audio_codec_id;
}

int InputFormat::gopSize() const
{
    return m_gopSize;
}

AVFormatContext * InputFormat::formatContext() const
{
    return m_formatContext;
}

AVInputFormat * InputFormat::inputFormat() const
{
    return m_inputFormat;
}

AVStream * InputFormat::getStream(AVMediaType type, int * streamIndex) const
{
    for (unsigned int i = 0; i < m_formatContext->nb_streams; ++i)
    {
        if(!m_formatContext->streams[i] || !m_formatContext->streams[i]->codecpar)
            continue;

        if (m_formatContext->streams[i]->codecpar->codec_type == type)
        {
            if(streamIndex)
                *streamIndex = i;
            return m_formatContext->streams[i];
        }
    }
    return nullptr;
}

void InputFormat::calculateGopSize(const AVPacket * packet)
{
    if (m_gopSize)
        return;

    bool keyFrame = packet->flags & AV_PKT_FLAG_KEY;
    if (m_gopSearch)
    {
        if (keyFrame)
            m_gopSize = m_gopSearch;
        else
            ++m_gopSearch;
    }
    else if (keyFrame)
    {
        m_gopSearch = 1;
    }
}

} // namespace ffmpeg
} // namespace nx
