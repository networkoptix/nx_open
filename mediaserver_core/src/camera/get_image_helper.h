#pragma once

#include <memory>

#include <QByteArray>

#include <core/resource/resource_fwd.h>

#include <nx/api/mediaserver/image_request.h>

#include <nx/streaming/data_packet_queue.h>

#include <transcoding/filters/abstract_image_filter.h>

#include "camera_fwd.h"
#include "nx/streaming/media_data_packet.h"
#include <nx/mediaserver/server_module_aware.h>

class CLVideoDecoderOutput;
typedef CLVideoDecoderOutputPtr CLVideoDecoderOutputPtr;
class QnAbstractArchiveDelegate;
class QnMediaServerModule;

class QnGetImageHelper: public nx::mediaserver::ServerModuleAware
{
public:
    QnGetImageHelper(QnMediaServerModule* serverModule);

    CLVideoDecoderOutputPtr getImage(const nx::api::CameraImageRequest& request) const;

    QByteArray encodeImage(
        const CLVideoDecoderOutputPtr& outFrame, const QByteArray& format) const;

private:
    CLVideoDecoderOutputPtr readFrame(
        const nx::api::CameraImageRequest& request,
        bool usePrimaryStream,
        QnAbstractArchiveDelegate* archiveDelegate,
        int prefferedChannel,
        bool& isOpened) const;

    QSize updateDstSize(
        const QnVirtualCameraResourcePtr& camera,
        const QSize& dstSize,
        CLVideoDecoderOutputPtr outFrame,
        nx::api::ImageRequest::AspectRatio aspectRatio) const;

    CLVideoDecoderOutputPtr getImageWithCertainQuality(
        bool usePrimaryStream, const nx::api::CameraImageRequest& request) const;

    CLVideoDecoderOutputPtr decodeFrameFromCaches(
        QnVideoCameraPtr camera,
        bool usePrimaryStream,
        qint64 timestampUs,
        int preferredChannel,
        nx::api::ImageRequest::RoundMethod roundMethod) const;

    CLVideoDecoderOutputPtr decodeFrameSequence(
        std::unique_ptr<QnConstDataPacketQueue>& sequence, quint64 timestampUs) const;

    CLVideoDecoderOutputPtr decodeFrameFromLiveCache(
        bool usePrimaryStream, qint64 timestampUs, QnVideoCameraPtr camera) const;

    std::unique_ptr<QnConstDataPacketQueue> getLiveCacheGopTillTime(
        bool usePrimaryStream, qint64 timestampUs, QnVideoCameraPtr camera) const;
};
