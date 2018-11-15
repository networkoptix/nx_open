#pragma once

#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/uncompressed_video_frame.h>
#include <decoders/video/ffmpeg_video_decoder.h>

namespace nx {
namespace mediaserver {
namespace analytics {

class Yuv420UncompressedVideoFrame:
    public nxpt::CommonRefCounter<nx::sdk::analytics::UncompressedVideoFrame>
{
public:
    Yuv420UncompressedVideoFrame(CLConstVideoDecoderOutputPtr uncompressedFrame):
        m_frame(std::move(uncompressedFrame))
    {
    }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual int64_t timestampUsec() const override { return m_frame->pkt_dts; }
    virtual int width() const override { return m_frame->width; }
    virtual int height() const override { return m_frame->height; }
    virtual nx::sdk::Ratio sampleAspectRatio() const override { return {1, 1}; }
    virtual PixelFormat pixelFormat() const override { return PixelFormat::yuv420; }
    virtual Handle handleType() const override { return Handle::none; }
    virtual int handle() const override { return 0; }
    virtual int planeCount() const override { return 3; }
    virtual int dataSize(int plane) const override;
    virtual const char* data(int plane) const override;
    virtual int lineSize(int plane) const override;
    virtual bool map() const override { return false; }
    virtual void unmap() const override {}

private:
    bool validatePlane(int plane) const;

private:
    CLConstVideoDecoderOutputPtr m_frame;
};

} // namespace nx
} // namespace mediaserver
} // namespace nx
