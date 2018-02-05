#include "get_image_helper.h"

#include <camera/camera_pool.h>
#include <camera/video_camera.h>

#include <core/resource/camera_resource.h>

#include "utils/media/frame_info.h"
#include "transcoding/transcoder.h"
#include "transcoding/filters/tiled_image_filter.h"
#include "transcoding/filters/scale_image_filter.h"
#include "transcoding/filters/rotate_image_filter.h"


#include "plugins/resource/server_archive/server_archive_delegate.h"
#include <decoders/video/ffmpeg_video_decoder.h>
#include <nx/utils/log/log_main.h>
#include "media_server/media_server_module.h"

static const int MAX_GOP_LEN = 100;
static const nx::utils::log::Tag kLogTag(lit("QnGetImageHelper"));


QnCompressedVideoDataPtr getNextArchiveVideoPacket(QnAbstractArchiveDelegate* archiveDelegate, qint64 ceilTime)
{
    QnCompressedVideoDataPtr video;
    for (int i = 0; i < 20 && !video; ++i)
    {
        QnAbstractMediaDataPtr media = archiveDelegate->getNextData();
        if (!media || media->timestamp == DATETIME_NOW)
            break;
        video = std::dynamic_pointer_cast<QnCompressedVideoData>(media);
    }

    // if ceilTime specified try frame with time > requested time (round time to ceil)
    if (ceilTime != (qint64)AV_NOPTS_VALUE && video && video->timestamp < ceilTime - 1000ll)
    {
        for (int i = 0; i < MAX_GOP_LEN; ++i)
        {
            QnAbstractMediaDataPtr media2 = archiveDelegate->getNextData();
            if (!media2 || media2->timestamp == DATETIME_NOW)
                break;
            QnCompressedVideoDataPtr video2 = std::dynamic_pointer_cast<QnCompressedVideoData>(media2);
            if (video2 && (video2->flags & AV_PKT_FLAG_KEY))
                return video2;
        }
    }

    return video;
}

QSize QnGetImageHelper::updateDstSize(
    const QnVirtualCameraResourcePtr& camera,
    const QSize& srcSize,
    QSharedPointer<CLVideoDecoderOutput> outFrame,
    nx::api::ImageRequest::AspectRatio aspectRatio)
{
    QSize dstSize(srcSize);
    double sar = outFrame->sample_aspect_ratio;
    double ar = sar * outFrame->width / outFrame->height;
    if (!dstSize.isEmpty())
    {
        dstSize.setHeight(qPower2Ceil((unsigned)dstSize.height(), 4));
        dstSize.setWidth(qPower2Ceil((unsigned)dstSize.width(), 4));
    }
    else if (dstSize.height() > 0)
    {
        dstSize.setHeight(qPower2Ceil((unsigned)dstSize.height(), 4));
        dstSize.setWidth(qPower2Ceil((unsigned)(dstSize.height()*ar), 4));
    }
    else if (dstSize.width() > 0)
    {
        dstSize.setWidth(qPower2Ceil((unsigned)dstSize.width(), 4));
        dstSize.setHeight(qPower2Ceil((unsigned)(dstSize.width() / ar), 4));
    }
    else
    {
        dstSize = QSize(outFrame->width * sar, outFrame->height);
    }
    static constexpr int kMaxSize = nx::api::CameraImageRequest::kMaximumSize;
    dstSize.setWidth(qMin(dstSize.width(), qMax(kMaxSize, outFrame->width) * sar));
    dstSize.setHeight(qMin(dstSize.height(), qMax(kMaxSize, outFrame->height)));

    if (aspectRatio == nx::api::ImageRequest::AspectRatio::custom)
    {
        const qreal customAr = camera->customAspectRatio();
        if (!qFuzzyIsNull(customAr))
            dstSize.setWidth(dstSize.height() * customAr);
    }

    return QnCodecTranscoder::roundSize(dstSize);
}


QSharedPointer<CLVideoDecoderOutput> QnGetImageHelper::readFrame(
    const nx::api::CameraImageRequest& request,
    bool useHQ,
    QnAbstractArchiveDelegate* archiveDelegate,
    int prefferedChannel,
    bool& isOpened)
{
    const auto& resource = request.camera;
    qint64 timeUSec = request.msecSinceEpoch * 1000;

    auto openDelegateIfNeeded =
        [&](std::function<qint64()> positionUs)
        {
            if (isOpened)
                return;

            const bool canSeekImmediately = archiveDelegate->getFlags().
                testFlag(QnAbstractArchiveDelegate::Flag_CanSeekImmediatly);

            if (canSeekImmediately)
            {
                archiveDelegate->seek(positionUs(), true);
                isOpened = archiveDelegate->open(resource, /*archiveIntegrityWatcher*/ nullptr);
            }
            else
            {
                isOpened = archiveDelegate->open(resource, /*archiveIntegrityWatcher*/ nullptr);
                if (isOpened)
                    archiveDelegate->seek(positionUs(), true);
            }
        };

    auto camera = qnCameraPool->getVideoCamera(resource);

    CLVideoDecoderOutputPtr outFrame(new CLVideoDecoderOutput());
    QnConstCompressedVideoDataPtr video;
    bool isArchiveVideoPacket = false;
    if (request.msecSinceEpoch == DATETIME_NOW)
    {
        // get live data
        if (camera)
            video = camera->getLastVideoFrame(useHQ, prefferedChannel);
    }
    else if (request.msecSinceEpoch == nx::api::ImageRequest::kLatestThumbnail)
    {
        // get latest data
        if (camera)
        {
            video = camera->getLastVideoFrame(useHQ, prefferedChannel);
            if (!video)
                video = camera->getLastVideoFrame(!useHQ, prefferedChannel);
        }
        if (!video)
        {
            openDelegateIfNeeded([&]() { return archiveDelegate->endTime() - 1000 * 100; });
            video = getNextArchiveVideoPacket(archiveDelegate, AV_NOPTS_VALUE);
            isArchiveVideoPacket = true;
        }
    }
    else
    {
        isArchiveVideoPacket = true;
        // get archive data
        openDelegateIfNeeded([&]() { return timeUSec; });
        // todo: getNextArchiveVideoPacket should be refactored to videoSequence interface
        video = getNextArchiveVideoPacket(
            archiveDelegate,
            request.roundMethod == nx::api::ImageRequest::RoundMethod::iFrameAfter
                ? request.msecSinceEpoch
                : AV_NOPTS_VALUE);
        if (!video && camera)
        {

            // try approx frame from GOP keeper
            auto videoSequence = camera->getFrameSequenceByTime(
                useHQ,
                timeUSec,
                prefferedChannel,
                request.roundMethod);
            if (!videoSequence)
            {
                videoSequence = camera->getFrameSequenceByTime(
                    !useHQ, //< if not found try alternate quality
                    timeUSec,
                    prefferedChannel,
                    request.roundMethod);
            }
            if (videoSequence && videoSequence->size() > 0)
                return decodeFrameSequence(videoSequence, timeUSec);
        }
    }

    if (!video)
        return CLVideoDecoderOutputPtr();

    QnFfmpegVideoDecoder decoder(video->compressionType, video, false);
    bool gotFrame = false;

    if (!isArchiveVideoPacket)
    {
        if (resource->getStatus() == Qn::Online
            || resource->getStatus() == Qn::Recording
            || request.msecSinceEpoch != DATETIME_NOW)
        {
            gotFrame = decoder.decode(video, &outFrame);
            if (!gotFrame)
                gotFrame = decoder.decode(video, &outFrame); // decode twice
        }
    }
    else
    {
        bool precise = request.roundMethod == nx::api::ImageRequest::RoundMethod::precise;
        for (int i = 0; i < MAX_GOP_LEN && !gotFrame && video; ++i)
        {
            gotFrame = decoder.decode(video, &outFrame) && (!precise || video->timestamp >= timeUSec);
            if (gotFrame)
                break;
            video = getNextArchiveVideoPacket(archiveDelegate, AV_NOPTS_VALUE);
        }
    }

    if (video)
        outFrame->channel = video->channelNumber;

    return outFrame;
}

QSharedPointer<CLVideoDecoderOutput> QnGetImageHelper::getImage(
    const nx::api::CameraImageRequest& request)
{
    if (!request.camera)
        return QSharedPointer<CLVideoDecoderOutput>();

    QSize dstSize(request.size);
#ifdef EDGE_SERVER
    if (dstSize.height() < 1)
        dstSize.setHeight(360); //on edge instead of full-size image we return 360p
#endif
    static constexpr int kMinSize = nx::api::CameraImageRequest::kMinimumSize;
    //if requested size is less than kMinSize in any dimension then upscaling to kMinSize
    if (dstSize.height() < kMinSize && dstSize.height() > 0)
    {
        dstSize.setWidth(dstSize.width() * kMinSize / dstSize.height());
        dstSize.setHeight(kMinSize);
    }
    if (dstSize.width() < kMinSize && dstSize.width() > 0)
    {
        dstSize.setWidth(kMinSize);
        dstSize.setHeight(dstSize.height() * kMinSize / dstSize.width());
    }

    bool useHQ = true;
    if ((dstSize.width() > 0 && dstSize.width() <= 480) || (dstSize.height() > 0 && dstSize.height() <= 316))
        useHQ = false;

    auto frame = getImageWithCertainQuality(useHQ, request);
    if (frame)
        return frame;
    //trying different quality if could not find frame
    return getImageWithCertainQuality(!useHQ, request);
}

AVPixelFormat updatePixelFormat(AVPixelFormat fmt)
{
    switch (fmt)
    {
        case AV_PIX_FMT_YUV420P:
            return AV_PIX_FMT_YUVJ420P;
        case AV_PIX_FMT_YUV422P:
            return AV_PIX_FMT_YUVJ422P;
        case AV_PIX_FMT_YUV444P:
            return AV_PIX_FMT_YUVJ444P;
        default:
            return fmt;
    }
}

QByteArray QnGetImageHelper::encodeImage(const QSharedPointer<CLVideoDecoderOutput>& outFrame, const QByteArray& format)
{
    QByteArray result;

    AVCodecContext* videoEncoderCodecCtx = avcodec_alloc_context3(0);
    videoEncoderCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    auto codecId = (format == "jpg" || format == "jpeg") ? AV_CODEC_ID_MJPEG : AV_CODEC_ID_PNG;
    videoEncoderCodecCtx->codec_id = codecId;
    auto pixelFormat = updatePixelFormat((AVPixelFormat)outFrame->format);
    videoEncoderCodecCtx->pix_fmt = pixelFormat;
    videoEncoderCodecCtx->width = outFrame->width;
    videoEncoderCodecCtx->height = outFrame->height;
    videoEncoderCodecCtx->bit_rate = outFrame->width * outFrame->height;
    videoEncoderCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    videoEncoderCodecCtx->time_base.num = 1;
    videoEncoderCodecCtx->time_base.den = 30;

    AVCodec* codec = avcodec_find_encoder_by_name(format == "jpg" || format == "jpeg" ? "mjpeg" : format.constData());
    if (avcodec_open2(videoEncoderCodecCtx, codec, NULL) < 0)
    {
        NX_WARNING(kLogTag,
            lm("Can't initialize ffmpeg encoder to encode image. codec=%1, pixel format=%2, size=%3x%4")
            .arg(codecId)
            .arg(pixelFormat)
            .arg(outFrame->width).arg(outFrame->height));
    }
    else
    {
        const int MAX_VIDEO_FRAME = outFrame->width * outFrame->height * 3 / 2;
        quint8* m_videoEncodingBuffer = (quint8*)qMallocAligned(MAX_VIDEO_FRAME, 32);
        //int encoded = avcodec_encode_video(videoEncoderCodecCtx, m_videoEncodingBuffer, MAX_VIDEO_FRAME, outFrame.data());
        QnFfmpegAvPacket outPacket(m_videoEncodingBuffer, MAX_VIDEO_FRAME);
        int got_packet = 0;
        int encodeResult = avcodec_encode_video2(videoEncoderCodecCtx, &outPacket, outFrame.data(), &got_packet);
        if (encodeResult == 0 && got_packet)
        {
            result.append((const char*)m_videoEncodingBuffer, outPacket.size);
        }
        else
        {
            NX_WARNING(kLogTag,
                lm("Can't encode image. codec=%1, pixel format=%2, size=%3x%4, errCode=%5")
                .arg(codecId)
                .arg(pixelFormat)
                .arg(outFrame->width).arg(outFrame->height)
                .arg(encodeResult));
        }

        qFreeAligned(m_videoEncodingBuffer);
    }
    QnFfmpegHelper::deleteAvCodecContext(videoEncoderCodecCtx);

    return result;
}

QSharedPointer<CLVideoDecoderOutput> QnGetImageHelper::getImageWithCertainQuality(bool useHQ,
    const nx::api::CameraImageRequest& request)
{
    const auto camera = request.camera;

    QSize dstSize = request.size;

    int rotation = request.rotation;
    if (rotation == nx::api::ImageRequest::kDefaultRotation)
        rotation = camera->getProperty(QnMediaResource::rotationKey()).toInt();

    QnConstResourceVideoLayoutPtr layout = camera->getVideoLayout();

    std::unique_ptr<QnAbstractArchiveDelegate> archiveDelegate(camera->createArchiveDelegate());
    if (!archiveDelegate)
        archiveDelegate.reset(new QnServerArchiveDelegate(qnServerModule)); // default value
    archiveDelegate->setPlaybackMode(PlaybackMode::ThumbNails);
    bool isOpened = false;

    if (!useHQ)
        archiveDelegate->setQuality(MEDIA_Quality_Low, true, QSize());

    QList<QnAbstractImageFilterPtr> filterChain;

    CLVideoDecoderOutputPtr outFrame;
    int channelMask = (1 << layout->channelCount()) - 1;
    bool gotNullFrame = false;
    for (int i = 0; i < layout->channelCount(); ++i)
    {
        CLVideoDecoderOutputPtr frame = readFrame(
            request,
            useHQ,
            archiveDelegate.get(),
            i,
            isOpened);
        if (!frame)
        {
            gotNullFrame = true;
            if (i == 0)
                return QSharedPointer<CLVideoDecoderOutput>();
            else
                continue;
        }
        channelMask &= ~(1 << frame->channel);
        if (i == 0)
        {
            dstSize = updateDstSize(camera, dstSize, frame, request.aspectRatio);
            if (dstSize.width() <= 16 || dstSize.height() <= 8)
                return CLVideoDecoderOutputPtr();
            filterChain << QnAbstractImageFilterPtr(new QnScaleImageFilter(dstSize));
            filterChain << QnAbstractImageFilterPtr(new QnTiledImageFilter(layout));
            filterChain << QnAbstractImageFilterPtr(new QnRotateImageFilter(rotation));
        }
        for (auto filter : filterChain)
        {
            frame = filter->updateImage(frame);
            if (!frame)
                break;
        }
        if (frame)
            outFrame = frame;
    }
    // read more archive frames to get all channels for pano cameras
    if (channelMask && !gotNullFrame)
    {
        for (int i = 0; i < 10; ++i)
        {
            CLVideoDecoderOutputPtr frame = readFrame(
                request,
                useHQ,
                archiveDelegate.get(),
                0,
                isOpened);
            if (frame)
            {
                channelMask &= ~(1 << frame->channel);
                for (auto filter : filterChain)
                {
                    frame = filter->updateImage(frame);
                    if (!frame)
                        break;
                }
                outFrame = frame;
            }
        }
    }
    return outFrame;
}

CLVideoDecoderOutputPtr QnGetImageHelper::decodeFrameSequence(
    std::unique_ptr<QnConstDataPacketQueue>& sequence,
    quint64 timeUSec)
{
    if (!sequence || sequence->isEmpty())
        return CLVideoDecoderOutputPtr();

    bool gotFrame = false;
    auto randomAccess = sequence->lock();
    auto firstFrame = std::dynamic_pointer_cast<const QnCompressedVideoData>(randomAccess.at(0));

    if (!firstFrame)
        return CLVideoDecoderOutputPtr();

    CLVideoDecoderOutputPtr outFrame(new CLVideoDecoderOutput());
    QnFfmpegVideoDecoder decoder(firstFrame->compressionType, firstFrame, false);

    for (auto i = 0; i < randomAccess.size() && !gotFrame; ++i)
    {
        auto frame = std::dynamic_pointer_cast<const QnCompressedVideoData>(randomAccess.at(i));
        gotFrame = decoder.decode(frame, &outFrame);
        if (frame->timestamp >= timeUSec)
            break;
    }
    while (decoder.decode(QnConstCompressedVideoDataPtr(), &outFrame))
        gotFrame = true; //< flush decoder buffer
    return gotFrame ? outFrame : CLVideoDecoderOutputPtr();
}
