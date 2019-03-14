#pragma once

#include <vector>

#include <nx/utils/log/assert.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>

namespace nx::vms::server::analytics {

class GenericUncompressedVideoFrame:
    public nx::sdk::RefCountable<nx::sdk::analytics::IUncompressedVideoFrame>
{
public:
    GenericUncompressedVideoFrame(
        int64_t timestampUs,
        int width,
        int height,
        PixelFormat pixelFormat,
        std::vector<std::vector<char>>&& data,
        std::vector<int>&& lineSizes)
        :
        m_timestampUs(timestampUs),
        m_width(width),
        m_height(height),
        m_pixelFormat(pixelFormat),
        m_data(std::move(data)),
        m_lineSizes(std::move(lineSizes))
    {
        NX_ASSERT(!m_data.empty());
        NX_ASSERT(m_lineSizes.size() == m_data.size());
    }

    virtual int64_t timestampUs() const override { return m_timestampUs; }
    virtual int width() const override { return m_width; }
    virtual int height() const override { return m_height; }
    virtual PixelAspectRatio pixelAspectRatio() const override { return {1, 1}; }
    virtual PixelFormat pixelFormat() const override { return m_pixelFormat; }
    virtual int planeCount() const override { return (int) m_data.size(); }
    virtual int dataSize(int plane) const override;
    virtual const char* data(int plane) const override;
    virtual int lineSize(int plane) const override;

private:
    bool validatePlane(int plane) const;

private:
    const int64_t m_timestampUs = 0;
    const int m_width = 0;
    const int m_height = 0;
    const PixelFormat m_pixelFormat = PixelFormat::yuv420;
    const std::vector<std::vector<char>> m_data;
    const std::vector<int> m_lineSizes;
};

} // namespace nx::vms::server::analytics
