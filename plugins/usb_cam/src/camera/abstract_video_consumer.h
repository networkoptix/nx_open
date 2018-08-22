#pragma once

#include "stream_consumer.h"

#include <memory>

#include "video_stream.h"
#include "codec_parameters.h"

namespace nx {
namespace usb_cam {

class AbstractVideoConsumer : public VideoConsumer
{
public:
    AbstractVideoConsumer(
        const std::weak_ptr<VideoStream>& streamReader,
        const CodecParameters& params);

    virtual int fps() const override;
    virtual void resolution(int *width, int *height) const override;
    virtual int bitrate() const override;

    virtual void setFps(int fps);
    virtual void setResolution(int width, int height);
    virtual void setBitrate(int bitrate);

protected:
    std::weak_ptr<VideoStream> m_streamReader;
    CodecParameters m_params;
};

} // namespace usb_cam
} // namespace nx