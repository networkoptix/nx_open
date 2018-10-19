#pragma once

#include <memory>

namespace nx {
namespace usb_cam {
namespace device {

class AbstractCompressionTypeDescriptor
{
public:    
    virtual nxcip::CompressionType toNxCompressionType() const = 0;
};

typedef std::shared_ptr<AbstractCompressionTypeDescriptor> CompressionTypeDescriptorPtr;

} // namespace device
} // namespace usb_cam
} // namespace nx