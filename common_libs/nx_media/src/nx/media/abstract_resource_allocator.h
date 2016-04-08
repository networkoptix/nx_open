#pragma once

namespace nx {
namespace media {

/*
* Helper class for decoders. Allow to execute lambda at GL thread.
* It helps to provide QVideoFrame with GL texture.
*/
class AbstractResourceAllocator
{
public:
    virtual void execAtGlThread(std::function<void (void*)> lambda, void* opaque) = 0;
    virtual void execAtGlThreadAsync(std::function<void()> lambda) = 0;
    virtual ~AbstractResourceAllocator() {}
};

}
}
