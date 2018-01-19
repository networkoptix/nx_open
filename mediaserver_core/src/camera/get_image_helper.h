#pragma once

#include <memory>

#include <QByteArray>

#include <core/resource/resource_fwd.h>

#include <nx/api/mediaserver/image_request.h>

#include <nx/streaming/data_packet_queue.h>

#include <transcoding/filters/abstract_image_filter.h>

#include "camera_fwd.h"

class CLVideoDecoderOutput;
typedef QSharedPointer<CLVideoDecoderOutput> CLVideoDecoderOutputPtr;
class QnAbstractArchiveDelegate;

class QnGetImageHelper
{
public:
    QnGetImageHelper() {}

    static QSharedPointer<CLVideoDecoderOutput> getImage(const nx::api::CameraImageRequest& request);

    static QByteArray encodeImage(const QSharedPointer<CLVideoDecoderOutput>& outFrame, const QByteArray& format);

private:
    static QSharedPointer<CLVideoDecoderOutput> readFrame(
        const nx::api::CameraImageRequest& request,
        bool useHQ,
        QnAbstractArchiveDelegate* archiveDelegate,
        int prefferedChannel,
        bool& isOpened);

    static QSize updateDstSize(
        const QnVirtualCameraResourcePtr& camera,
        const QSize& dstSize,
        QSharedPointer<CLVideoDecoderOutput> outFrame,
        nx::api::ImageRequest::AspectRatio aspectRatio);

    static QSharedPointer<CLVideoDecoderOutput> getImageWithCertainQuality(bool useHQ,
        const nx::api::CameraImageRequest& request);

    static QSharedPointer<CLVideoDecoderOutput> getMostPreciseImageFromLive(
        QnVideoCameraPtr& camera,
        bool useHq,
        quint64 time,
        int channel);

    static CLVideoDecoderOutputPtr decodeFrameSequence(
        std::unique_ptr<QnConstDataPacketQueue>& sequence,
        quint64 timeUSec);
};
