#pragma once

#include <vector>

#include <nx/utils/log/assert.h>
#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/uncompressed_video_frame.h>

namespace nx {
namespace sdk {
namespace metadata {

class GenericUncompressedVideoFrame: public nxpt::CommonRefCounter<UncompressedVideoFrame>
{
public:
    GenericUncompressedVideoFrame(
        int64_t timestampUsec,
        int width,
        int height,
        PixelFormat pixelFormat,
        std::vector<std::vector<char>>&& data,
        std::vector<int>&& lineSizes)
        :
        m_timestampUsec(timestampUsec),
        m_width(width),
        m_height(height),
        m_pixelFormat(pixelFormat),
        m_data(std::move(data)),
        m_lineSizes(std::move(lineSizes))
    {
        NX_ASSERT(!m_data.empty());
        NX_ASSERT(m_lineSizes.size() == m_data.size());
    }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual int64_t timestampUsec() const override { return m_timestampUsec; }
    virtual int width() const override { return m_width; }
    virtual int height() const override { return m_height; }
    virtual Ratio sampleAspectRatio() const override { return {1, 1}; }
    virtual PixelFormat pixelFormat() const override { return m_pixelFormat; }
    virtual Handle handleType() const override { return Handle::none; }
    virtual int handle() const override { return 0; }
    virtual int planeCount() const override { return (int) m_data.size(); }
    virtual int dataSize(int plane) const override;
    virtual const char* data(int plane) const override;
    virtual int lineSize(int plane) const override;
    virtual bool map() override { return false; }
    virtual void unmap() override {}

private:
    bool validatePlane(int plane) const;

private:
    const int64_t m_timestampUsec = 0;
    const int m_width = 0;
    const int m_height = 0;
    const PixelFormat m_pixelFormat = PixelFormat::yuv420;
    const std::vector<std::vector<char>> m_data;
    const std::vector<int> m_lineSizes;
};

} // namespace nx
} // namespace sdk
} // namespace nx
