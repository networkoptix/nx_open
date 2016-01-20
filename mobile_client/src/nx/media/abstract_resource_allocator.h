#pragma once

namespace nx
{
    namespace media
    {

        class AbstractResourceAllocator
        {
        public:
            virtual int newGlTexture() = 0;
            virtual QOpenGLContext* getGlContext() = 0;
            virtual ~AbstractResourceAllocator() {}
        };

    }
}
