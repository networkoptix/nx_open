#pragma once

#include <functional>
#include <memory>

namespace nx {
namespace media {

/**
 * Helper class for decoders. Allows to execute a lambda at GL thread.
 * It helps e.g. to provide QVideoFrame with GL texture.
 */
class AbstractResourceAllocator
{
public:
    virtual void execAtGlThread(std::function<void (void*)> lambda, void* opaque) = 0;

    virtual void execAtGlThreadAsync(std::function<void()> lambda) = 0;

    virtual ~AbstractResourceAllocator()
    {
    }
};

typedef std::shared_ptr<AbstractResourceAllocator> ResourceAllocatorPtr;

} // namespace media
} // namespace nx
