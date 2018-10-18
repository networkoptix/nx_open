#pragma once

#include <memory>

#include "codec_parameters.h"

namespace nx {
namespace usb_cam {

class VideoStream;

class AbstractVideoConsumer
{
public:
    AbstractVideoConsumer(
        const std::weak_ptr<VideoStream>& videoStream,
        const CodecParameters& params);

    virtual float fps() const;
    virtual void resolution(int *width, int *height) const;
    virtual int bitrate() const;

    virtual void setFps(float fps);
    virtual void setResolution(int width, int height);
    virtual void setBitrate(int bitrate);

protected:
    std::weak_ptr<VideoStream> m_videoStream;
    CodecParameters m_params;
};

} // namespace usb_cam
} // namespace nx