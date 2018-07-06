#pragma once

#include "options.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

class AVFormatContext;
class AVInputFormat;
class AVStream;

namespace nx {
namespace ffmpeg {

class InputFormat : Options
{
public:
    InputFormat();
    ~InputFormat();

    int initialize(const char * deviceType);
    int open(const char * url);
    void close();

    int readFrame(AVPacket * packet);

    int setFps(int fps);
    int setResolution(int width, int height);
    AVCodecID videoCodecID() const;
    AVCodecID audioCodecID() const;

    AVFormatContext * formatContext() const;
    AVInputFormat * inputFormat() const;

    AVStream * getStream(AVMediaType type, int *streamIndex = nullptr) const;

private:
    AVFormatContext * m_formatContext = nullptr;
    AVInputFormat * m_inputFormat = nullptr;
};

} // namespace ffmpeg
} // namespace nx
