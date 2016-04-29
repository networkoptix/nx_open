#pragma once

#include <memory>

#include "abstract_resource_allocator.h"

namespace nx {
namespace media {

class DecoderRegistrar
{
public:
    /** Should be called once from main(). */
    static void registerDecoders(std::shared_ptr<AbstractResourceAllocator> allocator);
};

} // namespace media
} // namespace nx
