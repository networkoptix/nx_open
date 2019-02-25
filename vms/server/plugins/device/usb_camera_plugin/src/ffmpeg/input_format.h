#pragma once

#include "options.h"

#include <string>
#include <chrono>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "packet.h"

namespace nx {
namespace usb_cam {
namespace ffmpeg {

class InputFormat: public Options
{
public:
    InputFormat();
    ~InputFormat();

    int initialize(const char * deviceType);
    int open(const char * url);
    void close();

    void dumpFormat(int streamIndex = 0, int isOutput = 0) const;

    int readFrame(AVPacket * packet);

    /**
     * If formatContext()->flags was set with AVFMT_FLAG_NONBLOCK, then this function can be called to
     * loop repeatedly for /a timeout milliseconds, with a 1 millisecond sleep between each attempt.
     * The default value is 0. if left at 0, then the function returns immediately.
     *
     * @param packet - the AVPacket to read into.
     * @param timeout - the timeout specifying when to return in the event of repeated failure.
     */
    int readFrameNonBlock(
        AVPacket * packet,
        const std::chrono::milliseconds& timeout = std::chrono::milliseconds(0));

    AVCodecID videoCodecId() const;
    AVCodecID audioCodecId() const;

    AVFormatContext * formatContext();
    AVInputFormat * inputFormat();

    const AVFormatContext * formatContext() const;
    const AVInputFormat * inputFormat() const;

    AVStream * stream(uint32_t index) const;
    AVStream * findStream(AVMediaType type, int *streamIndex = nullptr) const;

    int setFps(float fps);
    int setResolution(int width, int height);

private:
    AVFormatContext * m_formatContext = nullptr;
    AVInputFormat * m_inputFormat = nullptr;
    std::string m_url;
};

} // namespace ffmpeg
} // namespace usb_cam
} // namespace nx
