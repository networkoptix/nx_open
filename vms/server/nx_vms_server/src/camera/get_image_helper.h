#pragma once

#include <memory>

#include <QByteArray>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>

#include <nx/api/mediaserver/image_request.h>

#include <nx/streaming/data_packet_queue.h>

#include <transcoding/filters/abstract_image_filter.h>

#include "camera_fwd.h"
#include "nx/streaming/media_data_packet.h"
#include <nx/vms/server/server_module_aware.h>

class CLVideoDecoderOutput;
typedef CLVideoDecoderOutputPtr CLVideoDecoderOutputPtr;
class QnAbstractArchiveDelegate;
class QnMediaServerModule;

QSize updateDstSize(const QnVirtualCameraResource* camera,
    const QSize& dstSize,
    const CLVideoDecoderOutput& outFrame,
    nx::api::ImageRequest::AspectRatio aspectRatio);

class QnGetImageHelper: public /*mixin*/ nx::vms::server::ServerModuleAware
{
public:
    QnGetImageHelper(QnMediaServerModule* serverModule);

    CLVideoDecoderOutputPtr getImage(const nx::api::CameraImageRequest& request) const;

    QByteArray encodeImage(
        const CLVideoDecoderOutputPtr& outFrame, const QByteArray& format) const;

    nx::vms::api::StreamIndex determineStreamIndex(
		const nx::api::CameraImageRequest& request) const;

private:
    CLVideoDecoderOutputPtr readFrame(
        const nx::api::CameraImageRequest& request,
        nx::vms::api::StreamIndex streamIndex,
        QnAbstractArchiveDelegate* archiveDelegate,
        int prefferedChannel,
        bool& isOpened) const;

    CLVideoDecoderOutputPtr getImageWithCertainQuality(
        nx::vms::api::StreamIndex streamIndex,
		const nx::api::CameraImageRequest& request) const;

    CLVideoDecoderOutputPtr decodeFrameFromCaches(
        QnVideoCameraPtr camera,
        nx::vms::api::StreamIndex streamIndex,
        qint64 timestampUs,
        int preferredChannel,
        nx::api::ImageRequest::RoundMethod roundMethod) const;

    CLVideoDecoderOutputPtr decodeFrameSequence(
        const QnResourcePtr& resource,
        std::unique_ptr<QnConstDataPacketQueue>& sequence, quint64 timestampUs) const;

    CLVideoDecoderOutputPtr decodeFrameFromLiveCache(
        nx::vms::api::StreamIndex streamIndex,
		qint64 timestampUs,
		QnVideoCameraPtr camera) const;

    std::unique_ptr<QnConstDataPacketQueue> getLiveCacheGopTillTime(
        nx::vms::api::StreamIndex streamIndex,
		qint64 timestampUs,
		QnVideoCameraPtr camera) const;
};
