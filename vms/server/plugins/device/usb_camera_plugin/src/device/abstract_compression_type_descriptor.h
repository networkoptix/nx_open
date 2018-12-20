#pragma once

#include <memory>

#include <camera/camera_plugin_types.h>

namespace nx {
namespace usb_cam {
namespace device {

class AbstractCompressionTypeDescriptor
{
public:
    virtual ~AbstractCompressionTypeDescriptor() = default;
    virtual nxcip::CompressionType toNxCompressionType() const = 0;
};

typedef std::shared_ptr<AbstractCompressionTypeDescriptor> CompressionTypeDescriptorPtr;

} // namespace device
} // namespace usb_cam
} // namespace nx
