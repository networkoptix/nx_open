#include "ffmpeg_image_provider.h"

#include <client/client_globals.h>

#include <core/resource/resource.h>

#include <decoders/video/ffmpeg_video_decoder.h>

#include <core/resource/avi/avi_resource.h>
#include <core/resource/avi/avi_archive_delegate.h>

#include <utils/media/frame_info.h>

QnFfmpegImageProvider::QnFfmpegImageProvider(const QnResourcePtr &resource, QObject* parent /* = NULL*/) :
    base_type(parent),
    m_resource(resource),
    m_image(QImage())
{
    connect(this, &QnFfmpegImageProvider::loadDelayed, this, &QnImageProvider::imageChanged, Qt::QueuedConnection);
}

QImage QnFfmpegImageProvider::image() const {
    return m_image;
}

QSize QnFfmpegImageProvider::sizeHint() const
{
    NX_ASSERT(false);
    return m_image.size();
}

Qn::ThumbnailStatus QnFfmpegImageProvider::status() const
{
    NX_ASSERT(false);
    return Qn::ThumbnailStatus::Invalid;
}

void QnFfmpegImageProvider::doLoadAsync()
{

    QnAviArchiveDelegatePtr archiveDelegate(new QnAviArchiveDelegate());
    if (!archiveDelegate->open(m_resource, /*archiveIntegrityWatcher*/ nullptr))
        return;

    /* This is required to correctly get file duration. */
    if (!archiveDelegate->findStreams())
        return;

    /* Positioning to the middle of the file because starting frames are often just black. */
    qint64 startTime = archiveDelegate->startTime();
    qint64 endTime = archiveDelegate->endTime();
    if (endTime > 0)
        archiveDelegate->seek( (startTime + endTime) / 2, true);

    /* Loading frame. */
    auto data = archiveDelegate->getNextData();
    if (!data)
        return;

    QnCompressedVideoDataPtr frame = std::dynamic_pointer_cast<QnCompressedVideoData>(data);
    if (!frame)
        return;

    /* Preparing frame to correctly decode it as a screenshot. */
    frame->flags |= QnAbstractMediaData::MediaFlags_DecodeTwice | QnAbstractMediaData::MediaFlags_StillImage;

    QSharedPointer<CLVideoDecoderOutput> outFrame( new CLVideoDecoderOutput() );
    QnFfmpegVideoDecoder decoder(frame->compressionType, frame, false);
    if (!decoder.decode(frame, &outFrame))
        return;

    m_image = outFrame->toImage();
    emit loadDelayed(m_image);
}
