#pragma once

#include <nx/sdk/analytics/i_uncompressed_media_frame.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Each class that implements IUncompressedVideoFrame interface should properly handle this GUID in
 * its queryInterface().
 */
static const nxpl::NX_GUID IID_UncompressedVideoFrame =
    {{0x46,0xb3,0x52,0x7f,0x17,0xf1,0x4e,0x29,0x98,0x6f,0xfa,0x1a,0xcc,0x87,0xac,0x0d}};

class IUncompressedVideoFrame: public IUncompressedMediaFrame
{
public:
    enum class PixelFormat
    {
        yuv420,
        argb,
        abgr,
        rgba,
        bgra,
        rgb,
        bgr,
        count
    };

    struct PixelAspectRatio
    {
        int numerator;
        int denominator;
    };

    /**
     * @return width of decoded frame in pixels.
     */
    virtual int width() const = 0;

    /**
     * @return Height of the decoded frame in pixels.
     */
    virtual int height() const = 0;

    /**
     * @return Aspect ratio of a frame pixel.
     */
    virtual PixelAspectRatio pixelAspectRatio() const = 0;

    virtual PixelFormat pixelFormat() const = 0;

    /**
     * @param plane Number of the plane, in range 0..planesCount().
     * @return Number of bytes in each pixel line of the plane, or 0 if the data is not accessible.
     */
    virtual int lineSize(int plane) const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
