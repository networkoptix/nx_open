#pragma once

#include "stream_reader.h"

#include <memory>

namespace nx {
namespace ffmpeg {

class AbstractPacketConsumer : public PacketConsumer
{
public:
    AbstractPacketConsumer(
        const std::weak_ptr<StreamReader>& streamReader,
        const CodecParameters& params);

    virtual int fps() const override;
    virtual void resolution(int *width, int *height) const override;
    virtual int bitrate() const override;

    virtual void setFps(int fps);
    virtual void setResolution(int width, int height);
    virtual void setBitrate(int bitrate);

protected:
    std::weak_ptr<StreamReader> m_streamReader;
    CodecParameters m_params;
};

class AbstractFrameConsumer : public FrameConsumer
{
public:
    AbstractFrameConsumer(
        const std::weak_ptr<StreamReader>& streamReader,
        const CodecParameters& params);

    virtual int fps() const override;
    virtual void resolution(int *width, int *height) const override;
    virtual int bitrate() const override;

    virtual void setFps(int fps);
    virtual void setResolution(int width, int height);
    virtual void setBitrate(int bitrate);

protected:
    std::weak_ptr<StreamReader> m_streamReader;
    CodecParameters m_params;
};

}
}