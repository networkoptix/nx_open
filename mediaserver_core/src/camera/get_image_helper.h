#pragma once

#include <memory>

#include <QByteArray>

#include <core/resource/resource_fwd.h>

#include <nx/api/mediaserver/image_request.h>

#include <nx/streaming/data_packet_queue.h>

#include <transcoding/filters/abstract_image_filter.h>

#include "camera_fwd.h"
#include "nx/streaming/media_data_packet.h"

class CLVideoDecoderOutput;
typedef CLVideoDecoderOutputPtr CLVideoDecoderOutputPtr;
class QnAbstractArchiveDelegate;

class QnGetImageHelper
{
public:
    QnGetImageHelper() {}

    static CLVideoDecoderOutputPtr getImage(const nx::api::CameraImageRequest& request);

    static QByteArray encodeImage(
        const CLVideoDecoderOutputPtr& outFrame, const QByteArray& format);

private:
    static CLVideoDecoderOutputPtr readFrame(
        const nx::api::CameraImageRequest& request,
        bool usePrimaryStream,
        QnAbstractArchiveDelegate* archiveDelegate,
        int prefferedChannel,
        bool& isOpened);

    static QSize updateDstSize(
        const QnVirtualCameraResourcePtr& camera,
        const QSize& dstSize,
        CLVideoDecoderOutputPtr outFrame,
        nx::api::ImageRequest::AspectRatio aspectRatio);

    static CLVideoDecoderOutputPtr getImageWithCertainQuality(
        bool usePrimaryStream, const nx::api::CameraImageRequest& request);

    static CLVideoDecoderOutputPtr decodeFrameFromCaches(
        QnVideoCameraPtr camera,
        bool usePrimaryStream,
        qint64 timestampUs,
        int preferredChannel,
        nx::api::ImageRequest::RoundMethod roundMethod);

    static CLVideoDecoderOutputPtr decodeFrameSequence(
        std::unique_ptr<QnConstDataPacketQueue>& sequence, quint64 timestampUs);

    static CLVideoDecoderOutputPtr decodeFrameFromLiveCache(
        bool usePrimaryStream, qint64 timestampUs, QnVideoCameraPtr camera);

    static std::unique_ptr<QnConstDataPacketQueue> getLiveCacheGopTillTime(
        bool usePrimaryStream, qint64 timestampUs, QnVideoCameraPtr camera);
};
