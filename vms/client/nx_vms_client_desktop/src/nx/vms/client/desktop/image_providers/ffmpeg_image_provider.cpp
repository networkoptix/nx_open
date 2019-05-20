#include "ffmpeg_image_provider.h"

#include <limits>

#include <QtConcurrent>

#include <client/client_globals.h>
#include <core/resource/resource.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/avi/avi_archive_delegate.h>
#include <decoders/video/ffmpeg_video_decoder.h>
#include <utils/media/frame_info.h>
#include <utils/common/delayed.h>

#include <nx/client/core/utils/geometry.h>
#include <nx/utils/guarded_callback.h>

// TODO: #vkutin #gdm Support customAspectRatio and videoLayout.

namespace nx::vms::client::desktop {

using namespace std::chrono;
using nx::vms::client::core::Geometry;

struct FfmpegImageProvider::Private
{
    const QnResourcePtr resource;
    QImage image;
    Qn::ThumbnailStatus status = Qn::ThumbnailStatus::Invalid;
    microseconds position = -1ms;
    const QSize maximumSize;

    explicit Private(const QnResourcePtr& resource, microseconds position, const QSize& maxSize):
        resource(resource),
        position(position),
        maximumSize(
            maxSize.width() > 0 ? maxSize.width() : std::numeric_limits<int>::max(),
            maxSize.height() > 0 ? maxSize.height() : std::numeric_limits<int>::max())
    {
    }

    static QThreadPool* threadPool()
    {
        static QThreadPool staticThreadPool;
        return &staticThreadPool;
    }

    QSize boundedSize(const QSize& size) const
    {
        return Geometry::bounded(size, maximumSize, Qt::KeepAspectRatio).toSize();
    }
};

FfmpegImageProvider::FfmpegImageProvider(
    const QnResourcePtr& resource, const QSize& maximumSize, QObject* parent)
    :
    FfmpegImageProvider(resource, -1ms, maximumSize, parent)
{
}

FfmpegImageProvider::FfmpegImageProvider(
    const QnResourcePtr& resource, microseconds position, const QSize& maximumSize, QObject* parent)
    :
    base_type(parent),
    d(new Private(resource, position, maximumSize))
{
}

FfmpegImageProvider::~FfmpegImageProvider()
{
}

QImage FfmpegImageProvider::image() const
{
    return d->image;
}

QSize FfmpegImageProvider::sizeHint() const
{
    static constexpr QSize kDefaultSize(1920, 1080);
    return d->image.isNull() ? d->boundedSize(kDefaultSize) : d->image.size();
}

Qn::ThumbnailStatus FfmpegImageProvider::status() const
{
    return d->status;
}

void FfmpegImageProvider::load(bool sync)
{
    if (d->status == Qn::ThumbnailStatus::Loading)
        return;

    d->status = Qn::ThumbnailStatus::Loading;
    emit statusChanged(d->status);

    const auto callback = nx::utils::guarded(this,
        [this](const QImage& image)
        {
            if (d->status != Qn::ThumbnailStatus::Loading)
                return;

            const auto size = d->boundedSize(image.size());

            d->image = size != image.size()
                ? image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                : image;

            d->status = image.isNull()
                ? Qn::ThumbnailStatus::NoData
                : Qn::ThumbnailStatus::Loaded;

            emit imageChanged(d->image);
            emit sizeHintChanged(sizeHint());
            emit statusChanged(d->status);
        });

    auto future = QtConcurrent::run(d->threadPool(),
        [resource = d->resource, position = d->position, thread = thread(), callback]()
        {
            const auto image =
                [resource, position]() -> QImage
                {
                    QnAviArchiveDelegatePtr archiveDelegate(new QnAviArchiveDelegate());
                    if (!archiveDelegate->open(resource))
                        return {};

                    // This is required to correctly get file duration.
                    if (!archiveDelegate->findStreams())
                        return {};

                    if (position < 0ms) //< Seek to middle
                    {
                        const qint64 startTime = archiveDelegate->startTime();
                        const qint64 endTime = archiveDelegate->endTime();
                        if (endTime > 0)
                            archiveDelegate->seek((startTime + endTime) / 2, true);
                    }
                    else
                    {
                        archiveDelegate->seek(position.count(), false);
                    }

                    // Loading frame.
                    const auto data = archiveDelegate->getNextData();
                    if (!data)
                        return {};

                    const auto frame = std::dynamic_pointer_cast<QnCompressedVideoData>(data);
                    if (!frame)
                        return {};

                    // Preparing frame to correctly decode it as a screenshot.
                    frame->flags |= QnAbstractMediaData::MediaFlags_DecodeTwice
                        | QnAbstractMediaData::MediaFlags_StillImage;

                    QSharedPointer<CLVideoDecoderOutput> outFrame(new CLVideoDecoderOutput());
                    QnFfmpegVideoDecoder decoder(DecoderConfig(),
                        frame->compressionType, frame);

                    if (!decoder.decode(frame, &outFrame))
                        return {};

                    return outFrame->toImage();
                }();

            executeInThread(thread,
                [callback, image]() { callback(image); });
        });

    if (sync)
        future.waitForFinished();
}

} // namespace nx::vms::client::desktop

