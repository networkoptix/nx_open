#pragma once

#include <memory>

namespace nx {
namespace device {

class AbstractCompressionTypeDescriptor
{
public:    
    virtual nxcip::CompressionType toNxCompressionType() const = 0;
};

typedef std::shared_ptr<AbstractCompressionTypeDescriptor> CompressionTypeDescriptorPtr;

} // namespace device
} // namespace nx