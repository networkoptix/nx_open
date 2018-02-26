#pragma once

#include <vector>

#include <plugins/plugin_tools.h>
#include "uncompressed_video_frame.h"

namespace nx {
namespace sdk {
namespace metadata {

class CommonUncompressedVideoFrame: public nxpt::CommonRefCounter<UncompressedVideoFrame>
{
public:
    CommonUncompressedVideoFrame(int64_t timestampUsec, int width, int height):
        m_timestampUsec(timestampUsec), m_width(width), m_height(height)
    {
    }

    virtual ~CommonUncompressedVideoFrame() = default;

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual int64_t timestampUsec() const override { return m_timestampUsec; }
    virtual int width() const override { return m_width; }
    virtual int height() const override { return m_height; }
    virtual Ratio sampleAspectRatio() const override { return {1, 1}; }
    virtual PixelFormat pixelFormat() const override { return PixelFormat::yuv420; }
    virtual Handle handleType() const override { return Handle::none; }
    virtual int handle() const override { return 0; }
    virtual int planeCount() const override;
    virtual int dataSize(int plane) const override;
    virtual const char* data(int plane) const override;
    virtual int lineSize(int plane) const;
    virtual bool map() override { return false; }
    virtual void unmap() override {}

    void setOwnedData(
        std::vector<std::vector<char>> data, std::vector<int> lineSize)
    {
        m_externalData.clear();
        m_externalDataSize.clear();
        m_ownedData = std::move(data);
        m_lineSize = std::move(lineSize);
    }

    void setExternalData(
        std::vector<const char*> data, std::vector<int> dataSize, std::vector<int> lineSize)
    {
        m_ownedData.clear();
        m_externalData = std::move(data);
        m_externalDataSize = std::move(dataSize);
        m_lineSize = std::move(lineSize);
    }

private:
    bool checkPlane(int plane, const char* func) const;

private:
    const int64_t m_timestampUsec = 0;
    const int m_width = 0;
    const int m_height = 0;

    std::vector<std::vector<char>> m_ownedData;
    std::vector<const char*> m_externalData;
    std::vector<int> m_externalDataSize;
    std::vector<int> m_lineSize;
};

} // namespace nx
} // namespace sdk
} // namespace nx