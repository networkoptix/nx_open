#include "input_format.h"

#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

#include "error.h"

namespace nx {
namespace webcam_plugin {
namespace ffmpeg {

InputFormat::InputFormat():
    Options()
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

int InputFormat::setFps(int fps)
{
    return setEntry("framerate", std::to_string(fps).c_str());
}

int InputFormat::setResolution(int width, int height)
{
    return setEntry("video_size", (std::to_string(width) + "x" + std::to_string(height)).c_str());
}

AVFormatContext * InputFormat::formatContext() const
{
    return m_formatContext;
}

AVInputFormat * InputFormat::inputFormat() const
{
    return m_inputFormat;
}

} // namespace ffmpeg
} // namespace webcam_plugin
} // namespace nx
