#pragma once

#include <plugins/metadata/abstract_media_frame.h>
#include <plugins/metadata/utils.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements AbstractVideoFrame interface
 * should properly handle this GUID in its queryInterface method
 */
static const nxpl::NX_GUID IID_VideoFrame =
    {{0x46, 0xb3, 0x52, 0x7f, 0x17, 0xf1, 0x4e, 0x29, 0x98, 0x6f, 0xfa, 0x1a, 0xcc, 0x87, 0xac, 0x0d}};

/**
 * @brief The AbstractVideoFrame class represents interface of decoded video frame.
 */
class AbstractVideoFrame: public AbstractMediaFrame
{
public:

    enum class PixelFormat
    {
        yuv420,
        yuv422,
        yuv444,
        rgba,
        rgb,
        bgr,
        bgra,
    };

    enum class Handle
    {
        NoHandle,
        GLTexture,
        EGLImageHandle,
        UserHandle = 1000
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

    /**
     * @brief handle type
     */
    virtual Handle handleType() const = 0;

    /**
     * @brief Return handle number or 0 if handle is not used.
     */
    virtual int handle() const = 0;

    /**
     * @brief maps the contents of a video frame to system (CPU addressable) memory.
     * @return true if map success. If handle is not zero function 'bits' should be called only after map call.
     * otherwise function 'bits' returns zero.
     * If function handle
     */
    virtual bool map();
    virtual void unmap();
};

} // namespace metadata
} // namespace sdk
} // namespace nx
