#pragma once

#include <plugins/metadata/abstract_media_frame.h>
#include <plugins/metadata/utils.h>

namespace nx {
namespace sdk {
namespace metadata {

static const nxpl::NX_GUID IID_VideoFrame =
    {{0x46, 0xb3, 0x52, 0x7f, 0x17, 0xf1, 0x4e, 0x29, 0x98, 0x6f, 0xfa, 0x1a, 0xcc, 0x87, 0xac, 0x0d}};

class AbstractVideoFrame: public AbstractMediaFrame
{
public:
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual Ratio sampleAspectRatio() const = 0;
    virtual char* pixelFormat() const = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
