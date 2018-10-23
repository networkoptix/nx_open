#pragma once

#include "options.h"

#include <string>

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

    AVCodecID videoCodecID() const;
    AVCodecID audioCodecID() const;

    AVFormatContext * formatContext();
    AVInputFormat * inputFormat();

    const AVFormatContext * formatContext() const;
    const AVInputFormat * inputFormat() const;

    AVStream * stream(int index) const;
    AVStream * findStream(AVMediaType type, int *streamIndex = nullptr) const;

    int setFps(float fps);
    int setResolution(int width, int height);
    void resolution(int * width, int * height) const;

private:
    AVFormatContext * m_formatContext = nullptr;
    AVInputFormat * m_inputFormat = nullptr;
    std::string m_url;
};

} // namespace ffmpeg
} // namespace usb_cam
} // namespace nx
