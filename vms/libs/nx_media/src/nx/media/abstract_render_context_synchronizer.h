#pragma once

#include <functional>
#include <memory>

namespace nx {
namespace media {

/**
 * Helper class for decoders. Allows to execute a lambda at rendering thread.
 * It helps e.g. to provide QVideoFrame with GL texture.
 */
struct AbstractRenderContextSynchronizer
{
    virtual void execInRenderContext(std::function<void (void*)> lambda, void* opaque) = 0;

    virtual void execInRenderContextAsync(std::function<void()> lambda) = 0;

    virtual ~AbstractRenderContextSynchronizer()
    {
    }
};

} // namespace media
} // namespace nx
