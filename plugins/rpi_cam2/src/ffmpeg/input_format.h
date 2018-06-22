#pragma once

#include "options.h"

#include <string>

class AVFormatContext;
class AVInputFormat;

namespace nx {
namespace webcam_plugin {
namespace ffmpeg {

class InputFormat : Options
{
public:
    InputFormat();
    ~InputFormat();

    int initialize(const char * deviceType);
    int open(const char * url);
    void close();

    int setFps(int fps);
    int setResolution(int width, int height);

    AVFormatContext * formatContext() const;
    AVInputFormat * inputFormat() const;

private:
    AVFormatContext * m_formatContext = nullptr;
    AVInputFormat * m_inputFormat = nullptr;
};

} // namespace ffmpeg
} // namespace webcam_plugin
} // namespace nx
