#include "ffmpeg_image_provider.h"

#include <QtConcurrent>

#include <client/client_globals.h>
#include <core/resource/resource.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/avi/avi_archive_delegate.h>
#include <decoders/video/ffmpeg_video_decoder.h>
#include <utils/media/frame_info.h>
#include <utils/common/delayed.h>

#include <nx/utils/guarded_callback.h>
#include <nx/utils/scope_guard.h>

namespace nx::vms::client::desktop {

struct FfmpegImageProvider::Private
{
    const QnResourcePtr resource;
    QImage image;
    Qn::ThumbnailStatus status = Qn::ThumbnailStatus::Invalid;

    explicit Private(const QnResourcePtr& resource):
        resource(resource)
    {
    }

    static QThreadPool* threadPool()
    {
        static QThreadPool staticThreadPool;
        return &staticThreadPool;
    }
};

FfmpegImageProvider::FfmpegImageProvider(const QnResourcePtr& resource, QObject* parent):
    FfmpegImageProvider(resource, -1, parent)
{
}

FfmpegImageProvider::FfmpegImageProvider(const QnResourcePtr& resource, qint64 positionUsec, QObject* parent):
    base_type(parent),
    d(new Private(resource)),
    m_positionUsec(positionUsec)
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
    return d->image.size();
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

            d->image = image;
            d->status = image.isNull()
                ? Qn::ThumbnailStatus::NoData
                : Qn::ThumbnailStatus::Loaded;

            emit imageChanged(d->image);
            emit sizeHintChanged(sizeHint());
            emit statusChanged(d->status);
        });

    auto future = QtConcurrent::run(d->threadPool(),
        [resource = d->resource, positionUs = m_positionUsec, thread = thread(), callback]()
        {
            const auto image =
                [resource, positionUs]() -> QImage
                {
                    QnAviArchiveDelegatePtr archiveDelegate(new QnAviArchiveDelegate());
                    if (!archiveDelegate->open(resource))
                        return {};

                    // This is required to correctly get file duration.
                    if (!archiveDelegate->findStreams())
                        return {};

                    if (positionUs == -1) //< Seek to middle
                    {
                        const qint64 startTime = archiveDelegate->startTime();
                        const qint64 endTime = archiveDelegate->endTime();
                        if (endTime > 0)
                            archiveDelegate->seek((startTime + endTime) / 2, true);
                    }
                    else
                    {
                        archiveDelegate->seek(positionUs, false);
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
                    QnFfmpegVideoDecoder decoder(DecoderConfig::fromResource(resource),
                        frame->compressionType, frame, false);

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

