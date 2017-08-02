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

    /**
     * @return null terminated string describing pixel format.
     * Possible values are ... TODO: #dmishin insert possible formats.
     */
    virtual char* pixelFormat() const = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
