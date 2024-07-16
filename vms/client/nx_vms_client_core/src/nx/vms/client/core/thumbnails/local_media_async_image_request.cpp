// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_media_async_image_request.h"

#include <QtCore/QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <core/resource/avi/avi_archive_delegate.h>
#include <core/resource/resource.h>
#include <decoders/video/ffmpeg_video_decoder.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/common/utils/custom_thread_pool.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <utils/common/delayed.h>
#include <utils/media/frame_info.h>

namespace nx::vms::client::core {

namespace {

using namespace std::chrono;

QImage toImage(const CLVideoDecoderOutputPtr& frame, const QSize& size)
{
    if (frame->size() == size)
        return frame->toImage();

    if (const auto scaledFrame = frame->scaled(size))
        return scaledFrame->toImage();

    // If FFMPEG scaling fails, use QImage scaling.
    return frame->toImage().scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QImage getFrame(
    const QnResourcePtr& resource, std::chrono::milliseconds position, int maximumSize)
{
    QnAviArchiveDelegatePtr archiveDelegate(new QnAviArchiveDelegate());
    if (!archiveDelegate->open(resource))
        return {};

    if (position < 0ms) //< Seek to middle.
    {
        const qint64 startTime = archiveDelegate->startTime();
        const qint64 endTime = archiveDelegate->endTime();
        if (endTime > 0)
            archiveDelegate->seek((startTime + endTime) / 2, true);
    }
    else
    {
        archiveDelegate->seek(microseconds(position).count(), false);
    }

    // Loading frame.
    const auto data = archiveDelegate->getNextData();
    if (!data)
        return {};

    const auto frame = std::dynamic_pointer_cast<QnCompressedVideoData>(data);
    if (!frame)
        return {};

    // Preparing frame to correctly decode it as a screenshot.
    frame->flags |= QnAbstractMediaData::MediaFlags_StillImage;

    QSharedPointer<CLVideoDecoderOutput> outFrame(new CLVideoDecoderOutput());
    QnFfmpegVideoDecoder decoder(
        DecoderConfig(),
        /*metrics*/ nullptr,
        frame);

    if (!decoder.decode(frame, &outFrame))
        return {};

    const int effectiveMaximumSize = maximumSize > 0
        ? maximumSize
        : std::numeric_limits<int>::max();

    const auto size = nx::vms::client::core::Geometry::bounded(outFrame->size(),
        QSize(effectiveMaximumSize, effectiveMaximumSize), Qt::KeepAspectRatio).toSize();

    QImage image = toImage(outFrame, size);

    AsyncImageResult::setTimestamp(image, std::chrono::microseconds(outFrame->pkt_dts));
    return image;
}

} // namespace

struct LocalMediaAsyncImageRequest::Private
{
    QFutureWatcher<QImage> imageWatcher;

    QThreadPool* threadPool() const
    {
        return CustomThreadPool::instance("LocalMediaAsyncImageRequest_thread_pool");
    }
};

LocalMediaAsyncImageRequest::LocalMediaAsyncImageRequest(
    const QnResourcePtr& resource,
    std::chrono::milliseconds position,
    int maximumSize,
    QObject* parent)
    :
    base_type(parent),
    d(new Private())
{
    if (!NX_ASSERT(resource && resource->hasFlags(Qn::local_media)) || !NX_ASSERT(d->threadPool()))
    {
        setImage({});
        return;
    }

    NX_VERBOSE(this, "Loading thumbnail for %1 (%2)", resource->getName(),
        resource->getId());

    connect(&d->imageWatcher, &QFutureWatcherBase::finished, this,
        [this]() { setImage(d->imageWatcher.future().result()); });

    d->imageWatcher.setFuture(QtConcurrent::run(d->threadPool(),
        [resource, position, maximumSize]() -> QImage
        {
            return getFrame(resource, position, maximumSize);
        }));
}

LocalMediaAsyncImageRequest::~LocalMediaAsyncImageRequest()
{
    // Required here for forward-declared scoped pointer destruction.
}

} // namespace nx::vms::client::core
