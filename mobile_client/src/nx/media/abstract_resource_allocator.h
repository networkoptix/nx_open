#pragma once

namespace nx
{
    namespace media
    {

        class AbstractResourceAllocator
        {
        public:
            virtual void execAtGlThread(std::function<void (void*)> lambda, void* opaque) = 0;
            virtual ~AbstractResourceAllocator() {}
        };

    }
}
