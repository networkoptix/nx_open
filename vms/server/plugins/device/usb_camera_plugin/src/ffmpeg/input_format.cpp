#include "input_format.h"

#include <string>
#include <thread>

namespace nx {
namespace usb_cam {
namespace ffmpeg {

namespace {

static std::chrono::milliseconds now()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch());
}

}

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
        // There is no error code for format not found
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

void InputFormat::dumpFormat(int streamIndex, int isOutput) const
{
    av_dump_format(m_formatContext, streamIndex, m_url.c_str(), isOutput);
}

int InputFormat::readFrame(AVPacket * outPacket)
{
    return av_read_frame(m_formatContext, outPacket);
}

AVCodecID InputFormat::videoCodecId() const
{
    return m_formatContext->video_codec_id;
}

AVCodecID InputFormat::audioCodecId() const
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

AVStream * InputFormat::stream(uint32_t index) const
{
    if (index < 0 || index >= m_formatContext->nb_streams)
        return nullptr;
    return m_formatContext->streams[index];
}

AVStream * InputFormat::findStream(AVMediaType type, int * streamIndex) const
{
    for (unsigned int i = 0; i < m_formatContext->nb_streams; ++i)
    {
        if (!m_formatContext->streams[i] || !m_formatContext->streams[i]->codecpar)
            continue;

        if (m_formatContext->streams[i]->codecpar->codec_type == type)
        {
            if (streamIndex)
                *streamIndex = i;
            return m_formatContext->streams[i];
        }
    }
    return nullptr;
}

int InputFormat::setFps(float fps)
{
    return setEntry("framerate", std::to_string(fps).c_str());
}

int InputFormat::setResolution(int width, int height)
{
    return setEntry("video_size", (std::to_string(width) + "x" + std::to_string(height)).c_str());
}

} // namespace ffmpeg
} // namespace usb_cam
} // namespace nx
