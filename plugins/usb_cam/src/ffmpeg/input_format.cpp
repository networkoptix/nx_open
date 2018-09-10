#include "input_format.h"

#include <string>

namespace nx {
namespace ffmpeg {

InputFormat::InputFormat():
    Options(),
    m_actualTimePerFrame(0),
    m_lastPts(0)
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
        return AVERROR_PROTOCOL_NOT_FOUND;
    }

    m_formatContext = avformat_alloc_context();
    if (!m_formatContext)
        return AVERROR(ENOMEM);

    return 0;
}

int InputFormat::open(const char * url)
{
    m_url = url;
    int openCode = avformat_open_input(
        &m_formatContext,
        url,
        m_inputFormat,
        &m_options);

    if(m_formatContext && openCode >= 0)
        av_dump_format(m_formatContext, 0, url, 0);
    
    return openCode;
}

void InputFormat::close()
{
    if (m_formatContext)
        avformat_close_input(&m_formatContext);
}

int InputFormat::readFrame(AVPacket * outPacket)
{
    int result = av_read_frame(m_formatContext, outPacket);
    if (result >= 0)
        calculateActualTimePerFrame(outPacket);
    return result;
}

int InputFormat::setFps(float fps)
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

AVFormatContext * InputFormat::formatContext()
{
    return m_formatContext;
}

AVInputFormat * InputFormat::inputFormat()
{
    return m_inputFormat;
}

const AVFormatContext * InputFormat::formatContext() const
{
    return m_formatContext;
}

const AVInputFormat * InputFormat::inputFormat() const
{
    return m_inputFormat;
}

AVStream * InputFormat::stream(int index) const
{
    if (index < 0 || index >= m_formatContext->nb_streams)
        return nullptr;
    return m_formatContext->streams[index];
}

AVStream * InputFormat::findStream(AVMediaType type, int * streamIndex) const
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

void InputFormat::resolution(int * width, int * height) const
{
    AVStream * stream = findStream(AVMEDIA_TYPE_VIDEO);
    *width =  stream ? stream->codecpar->width : 0;
    *height = stream ? stream->codecpar->height : 0;
}

int InputFormat::actualTimePerFrame() const
{
    return m_actualTimePerFrame;
}

void InputFormat::calculateActualTimePerFrame(AVPacket * packet)
{
    if (!m_lastPts)
        m_lastPts = packet->pts;

    int actualTpf = packet->pts - m_lastPts;
    if (actualTpf > m_actualTimePerFrame)
        m_actualTimePerFrame = actualTpf;

    m_lastPts = packet->pts;
}

} // namespace ffmpeg
} // namespace nx
