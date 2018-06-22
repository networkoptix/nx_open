#include "input_format.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <string>

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
        // there is no error code for format not found
        return AVERROR_PROTOCOL_NOT_FOUND;

    m_formatContext = avformat_alloc_context();
    if (!m_formatContext)
        return AVERROR(ENOMEM);

    return 0;
}

int InputFormat::open(const char * url)
{
    return avformat_open_input(
        &m_formatContext,
        url,
        m_inputFormat,
        &m_options);
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
    std::string res = std::to_string(width) + "x" + std::to_string(height);
    return setEntry("video_size", res.c_str());
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
