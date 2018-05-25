#pragma once

#include <nx/sdk/metadata/media_frame.h>
#include <nx/sdk/common.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements VideoFrame interface should properly handle this GUID in its
 * queryInterface().
 */
static const nxpl::NX_GUID IID_UncompressedVideoFrame =
    {{0x46,0xb3,0x52,0x7f,0x17,0xf1,0x4e,0x29,0x98,0x6f,0xfa,0x1a,0xcc,0x87,0xac,0x0d}};

class UncompressedVideoFrame: public MediaFrame
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

    enum class Handle
    {
        none = 0,
        glTexture,
        eglImage,
        user = 1000
    };

    /**
     * @return width of decoded frame in pixels.
     */
    virtual int width() const = 0;

    /**
     * @return height of decoded frame in pixels.
     */
    virtual int height() const = 0;

    /**
     * @return aspect ratio of frame pixels.
     */
    virtual Ratio sampleAspectRatio() const = 0;

    virtual PixelFormat pixelFormat() const = 0;

    virtual Handle handleType() const = 0;

    /**
     * @return Handle, or 0 if there is no handle for this frame.
     */
    virtual int handle() const = 0;

    /**
     * @param plane Number of the plane, in range 0..planesCount().
     * @return Number of bytes in each pixel line of the plane, or 0 if the data is not accessible.
     */
    virtual int lineSize(int plane) const = 0;

    /**
     * Maps the raw data of the video frame to the system (CPU addressable) memory. If handle() is
     * non-zero, data() should be called only after map call, otherwise data() returns null.
     * @return Success.
     */
    virtual bool map() = 0;

    virtual void unmap() = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
