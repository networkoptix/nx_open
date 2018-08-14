#pragma once

namespace nx {
namespace device {

class AbstractCompressionTypeDescriptor
{
public:    
    virtual nxcip::CompressionType toNxCompressionType() const = 0;
};

} // namespace device
} // namespace nx