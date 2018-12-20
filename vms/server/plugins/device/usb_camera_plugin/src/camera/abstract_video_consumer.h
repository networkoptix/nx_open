#pragma once

#include <memory>

#include <camera/camera_plugin.h>

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
    virtual ~AbstractVideoConsumer() = default;

    virtual float fps() const;
    virtual nxcip::Resolution resolution() const;
    virtual int bitrate() const;

    virtual void setFps(float fps);
    virtual void setResolution(const nxcip::Resolution& resolution);
    virtual void setBitrate(int bitrate);

protected:
    std::weak_ptr<VideoStream> m_videoStream;
    CodecParameters m_params;
};

} // namespace usb_cam
} // namespace nx
