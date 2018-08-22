#pragma once

#include "stream_consumer.h"

#include <memory>

#include "video_stream_reader.h"
#include "codec_parameters.h"

namespace nx {
namespace ffmpeg {

class AbstractVideoConsumer : public VideoConsumer
{
public:
    AbstractVideoConsumer(
        const std::weak_ptr<VideoStreamReader>& streamReader,
        const CodecParameters& params);

    virtual int fps() const override;
    virtual void resolution(int *width, int *height) const override;
    virtual int bitrate() const override;

    virtual void setFps(int fps);
    virtual void setResolution(int width, int height);
    virtual void setBitrate(int bitrate);

protected:
    std::weak_ptr<VideoStreamReader> m_streamReader;
    CodecParameters m_params;
};

}
}