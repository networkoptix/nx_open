#pragma once

#include <utils/common/from_this_to_shared.h>

class QnAbstractVideoCamera:
    public QnFromThisToShared<QnAbstractVideoCamera>
{
public:
    virtual ~QnAbstractVideoCamera() = default;

    /**
     * Mark some camera activity (RTSP client connection for example).
     */
    virtual void inUse(void* user) = 0;
    /**
     * Unmark some camera activity (RTSP client connection for example).
     */
    virtual void notInUse(void* user) = 0;
};

