#include "ffmpeg_image_provider.h"

#include <client/client_globals.h>

#include <core/resource/resource.h>

#include <decoders/video/ffmpeg_video_decoder.h>

#include <core/resource/avi/avi_resource.h>
#include <core/resource/avi/avi_archive_delegate.h>

#include <utils/media/frame_info.h>
#include <utils/common/delayed.h>

namespace nx {
namespace client {
namespace desktop {

struct FfmpegImageProvider::Private
{
    const QnResourcePtr resource;
    QImage image;
    Qn::ThumbnailStatus status = Qn::ThumbnailStatus::Invalid;

    explicit Private(const QnResourcePtr& resource):
        resource(resource)
    {
    }
};

FfmpegImageProvider::FfmpegImageProvider(const QnResourcePtr& resource, QObject* parent)
    : FfmpegImageProvider(resource, -1, parent)
{
}

FfmpegImageProvider::FfmpegImageProvider(const QnResourcePtr& resource, qint64 positionUsec, QObject* parent)
    : base_type(parent), d(new Private(resource)), m_positionUsec(positionUsec)
{
    connect(this, &FfmpegImageProvider::loadDelayed, this, &ImageProvider::imageChanged,
        Qt::QueuedConnection);
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

void FfmpegImageProvider::doLoadAsync()
{
    d->status = Qn::ThumbnailStatus::NoData;
    executeDelayedParented([this]{emit statusChanged(d->status);}, this);

    QnAviArchiveDelegatePtr archiveDelegate(new QnAviArchiveDelegate());
    if (!archiveDelegate->open(d->resource))
        return;

    /* This is required to correctly get file duration. */
    if (!archiveDelegate->findStreams())
        return;

    if (m_positionUsec == -1) //< Seek to middle
    {
        const qint64 startTime = archiveDelegate->startTime();
        const qint64 endTime = archiveDelegate->endTime();
        if (endTime > 0)
            archiveDelegate->seek((startTime + endTime) / 2, true);
    } else
    {
        archiveDelegate->seek(m_positionUsec, false);
    }

    /* Loading frame. */
    const auto data = archiveDelegate->getNextData();
    if (!data)
        return;

    const auto frame = std::dynamic_pointer_cast<QnCompressedVideoData>(data);
    if (!frame)
        return;

    /* Preparing frame to correctly decode it as a screenshot. */
    frame->flags |=
        QnAbstractMediaData::MediaFlags_DecodeTwice | QnAbstractMediaData::MediaFlags_StillImage;

    QSharedPointer<CLVideoDecoderOutput> outFrame(new CLVideoDecoderOutput());
    QnFfmpegVideoDecoder decoder(frame->compressionType, frame, false);
    if (!decoder.decode(frame, &outFrame))
        return;

    d->image = outFrame->toImage();
    if (d->image.isNull())
        return;

    d->status = Qn::ThumbnailStatus::Loaded;
    executeDelayedParented(
        [this]
        {
            emit imageChanged(d->image);
            emit sizeHintChanged(sizeHint());
            emit statusChanged(d->status);
        }, this);
}

} // namespace desktop
} // namespace client
} // namespace nx

