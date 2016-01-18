#pragma once

class QVideoFrame;
typedef QSharedPointer<QVideoFrame> QnVideoFramePtr;
typedef QSharedPointer<const QVideoFrame> QnConstVideoFramePtr;

namespace nx
{
    namespace media
    {
        class AbstractResourceAllocator;
    }
}
