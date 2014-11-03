#ifndef __GET_IMAGE_HELPER_H__
#define __GET_IMAGE_HELPER_H__

#include "core/resource/camera_resource.h"

class CLVideoDecoderOutput;
class QnVirtualCameraResource;
class QnServerArchiveDelegate;

class QnGetImageHelper
{
public:
    enum RoundMethod { IFrameBeforeTime, Precise, IFrameAfterTime };
    static const qint64 LATEST_IMAGE = -1;

    QnGetImageHelper() {}
    static QSharedPointer<CLVideoDecoderOutput> getImage(const QnVirtualCameraResourcePtr& res, qint64 time, const QSize& size, RoundMethod roundMethod = IFrameBeforeTime, int rotation = -1);
private:
    static QSharedPointer<CLVideoDecoderOutput> readFrame(qint64 time, bool useHQ, RoundMethod roundMethod, const QSharedPointer<QnVirtualCameraResource>& res, QnServerArchiveDelegate& serverDelegate, int prefferedChannel);
    static QSize updateDstSize(const QSharedPointer<QnVirtualCameraResource>& res, const QSize& dstSize, QSharedPointer<CLVideoDecoderOutput> outFrame);
};

#endif // __GET_IMAGE_HELPER_H__
