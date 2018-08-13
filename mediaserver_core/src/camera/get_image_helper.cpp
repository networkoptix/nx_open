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

namespace {

static constexpr int kMaxGopLen = 100;

static constexpr int kRoundFactor = 4;

static const nx::utils::log::Tag kLogTag(lit("QnGetImageHelper"));

QnCompressedVideoDataPtr getNextArchiveVideoPacket(
    QnAbstractArchiveDelegate* archiveDelegate, qint64 ceilTimeUs)
{
    QnCompressedVideoDataPtr video;
    for (int i = 0; i < 20 && !video; ++i)
    {
        QnAbstractMediaDataPtr media = archiveDelegate->getNextData();
        if (!media || media->timestamp == DATETIME_NOW)
            break;
        video = std::dynamic_pointer_cast<QnCompressedVideoData>(media);
    }

    // If ceilTime specified try frame with time > requested time (round time to ceil).
    if (ceilTimeUs != (qint64) AV_NOPTS_VALUE && video && video->timestamp < ceilTimeUs - 1000LL)
    {
        for (int i = 0; i < kMaxGopLen; ++i)
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

} // namespace

QnGetImageHelper::QnGetImageHelper(QnMediaServerModule* serverModule):
    m_serverModule(serverModule)
{
}

QSize QnGetImageHelper::updateDstSize(
    const QnVirtualCameraResourcePtr& camera,
    const QSize& srcSize,
    CLVideoDecoderOutputPtr outFrame,
    nx::api::ImageRequest::AspectRatio aspectRatio) const
{
    QSize dstSize(srcSize);
    double sar = outFrame->sample_aspect_ratio;
    double ar = sar * outFrame->width / outFrame->height;
    if (!dstSize.isEmpty())
    {
        dstSize.setHeight(qPower2Ceil((unsigned)dstSize.height(), kRoundFactor));
        dstSize.setWidth(qPower2Ceil((unsigned)dstSize.width(), kRoundFactor));
    }
    else if (dstSize.height() > 0)
    {
        dstSize.setHeight(qPower2Ceil((unsigned)dstSize.height(), kRoundFactor));
        dstSize.setWidth(qPower2Ceil((unsigned)(dstSize.height()*ar), kRoundFactor));
    }
    else if (dstSize.width() > 0)
    {
        dstSize.setWidth(qPower2Ceil((unsigned)dstSize.width(), kRoundFactor));
        dstSize.setHeight(qPower2Ceil((unsigned)(dstSize.width() / ar), kRoundFactor));
    }
    else
    {
        dstSize = QSize(outFrame->width * sar, outFrame->height);
    }
    static constexpr int kMaxSize = nx::api::CameraImageRequest::kMaximumSize;
    dstSize.setWidth(qMin(dstSize.width(), qMax(kMaxSize, outFrame->width) * sar));
    dstSize.setHeight(qMin(dstSize.height(), qMax(kMaxSize, outFrame->height)));

    if (aspectRatio == nx::api::ImageRequest::AspectRatio::auto_)
    {
        const auto customAr = camera->customAspectRatio();
        if (customAr.isValid())
            dstSize.setWidth(dstSize.height() * customAr.toFloat());
    }

    return QnCodecTranscoder::roundSize(dstSize);
}

CLVideoDecoderOutputPtr QnGetImageHelper::readFrame(
    const nx::api::CameraImageRequest& request,
    bool usePrimaryStream,
    QnAbstractArchiveDelegate* archiveDelegate,
    int preferredChannel,
    bool& isOpened) const
{
    const auto& resource = request.camera;
    qint64 timestampUs = request.usecSinceEpoch;

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

    auto camera = m_serverModule->videoCameraPool()->getVideoCamera(resource);

    CLVideoDecoderOutputPtr outFrame(new CLVideoDecoderOutput());
    QnConstCompressedVideoDataPtr video;
    bool isArchiveVideoPacket = false;
    if (request.usecSinceEpoch == DATETIME_NOW)
    {
        if (camera)
        {
            video = camera->getLastVideoFrame(usePrimaryStream, preferredChannel);
            if (video)
            {
                NX_VERBOSE(kLogTag) << lm("%1(NOW): Got camera last frame").arg(__func__);
            }
            else
            {
                NX_VERBOSE(kLogTag) << lm("%1(NOW): Reject: camera last frame missing")
                    .arg(__func__);
            }
        }
        else
        {
            NX_VERBOSE(kLogTag) << lm("%1(NOW): Reject: camera missing").arg(__func__);
        }
    }
    else if (request.usecSinceEpoch == nx::api::ImageRequest::kLatestThumbnail)
    {
        NX_VERBOSE(kLogTag) << lm("%1(LATEST): Get latest data").args(__func__);
        if (camera)
        {
            video = camera->getLastVideoFrame(usePrimaryStream, preferredChannel);
            if (!video)
            {
                video = camera->getLastVideoFrame(!usePrimaryStream, preferredChannel);
                if (video)
                {
                    NX_VERBOSE(kLogTag) << lm(
                        "%1(LATEST): Got camera last frame for the alternate stream")
                        .arg(__func__);
                }
                else
                {
                    NX_VERBOSE(kLogTag) << lm(
                        "%1(LATEST): Missing camera last frame for both streams").arg(__func__);
                }
            }
            else
            {
                NX_VERBOSE(kLogTag) << lm(
                    "%1(LATEST): Got camera last frame for the requested stream").args(__func__);
            }
        }
        if (!video)
        {
            openDelegateIfNeeded([&]() { return archiveDelegate->endTime() - 1000 * 100; });
            video = getNextArchiveVideoPacket(archiveDelegate, AV_NOPTS_VALUE);
            if (video)
                NX_VERBOSE(kLogTag) << lm("%1(LATEST): Got from archive").arg(__func__);
            else
                NX_VERBOSE(kLogTag) << lm("%1(LATEST): Missing from archive").arg(__func__);
            isArchiveVideoPacket = true;
        }
    }
    else
    {
        isArchiveVideoPacket = true;
        // Get archive data.
        openDelegateIfNeeded([&]() { return timestampUs; });
        // TODO: getNextArchiveVideoPacket should be refactored to videoSequence interface.
        video = getNextArchiveVideoPacket(
            archiveDelegate,
            request.roundMethod == nx::api::ImageRequest::RoundMethod::iFrameAfter
                ? request.usecSinceEpoch
                : AV_NOPTS_VALUE);
        if (video)
            NX_VERBOSE(kLogTag) << lm("%1(%2 us): Got from archive").args(__func__, timestampUs);
        else
            NX_VERBOSE(kLogTag) << lm("%1(%2 us): Missing from archive").args(__func__, timestampUs);

        if (!video && camera)
        {
            return decodeFrameFromCaches(
                camera, usePrimaryStream, timestampUs, preferredChannel, request.roundMethod);
        }
    }

    if (!video)
        return CLVideoDecoderOutputPtr();

    QnFfmpegVideoDecoder decoder(video->compressionType, video, /*mtDecoding*/ false);
    bool gotFrame = false;

    if (!isArchiveVideoPacket)
    {
        if (resource->getStatus() == Qn::Online
            || resource->getStatus() == Qn::Recording
            || request.usecSinceEpoch != DATETIME_NOW)
        {
            gotFrame = decoder.decode(video, &outFrame);
            if (!gotFrame)
                gotFrame = decoder.decode(video, &outFrame); // decode twice
        }
    }
    else
    {
        bool precise = request.roundMethod == nx::api::ImageRequest::RoundMethod::precise;
        for (int i = 0; i < kMaxGopLen && !gotFrame && video; ++i)
        {
            gotFrame = decoder.decode(video, &outFrame) && (!precise || video->timestamp >= timestampUs);
            if (gotFrame)
                break;
            video = getNextArchiveVideoPacket(archiveDelegate, AV_NOPTS_VALUE);
        }
    }

    if (video)
        outFrame->channel = video->channelNumber;

    return outFrame;
}

CLVideoDecoderOutputPtr QnGetImageHelper::decodeFrameFromCaches(
    QnVideoCameraPtr camera,
    bool usePrimaryStream,
    qint64 timestampUs,
    int preferredChannel,
    nx::api::ImageRequest::RoundMethod roundMethod) const
{
    const auto logPrefix = lm("%1(%2, %3 us, channel: %4, %5):").args(__func__,
        usePrimaryStream ? "primary" : "secondary", timestampUs, preferredChannel, roundMethod);

    // Try GopKeeper.
    auto videoSequence = camera->getFrameSequenceByTime(
        usePrimaryStream, timestampUs, preferredChannel, roundMethod);
    if (videoSequence)
    {
        NX_VERBOSE(kLogTag) << logPrefix << "Got from GopKeeper";
        return decodeFrameSequence(videoSequence, timestampUs);
    }

    // Try liveCache.
    if (auto frame = decodeFrameFromLiveCache(usePrimaryStream, timestampUs, camera))
    {
        NX_VERBOSE(kLogTag) << logPrefix << lm("Got from liveCache: %1 us").arg(frame->pkt_dts);
        return frame;
    }

    NX_VERBOSE(kLogTag) << logPrefix << "Missing from GopKeeper for both streams and liveCache";
    return CLVideoDecoderOutputPtr();
}

CLVideoDecoderOutputPtr QnGetImageHelper::decodeFrameFromLiveCache(
    bool usePrimaryStream, qint64 timestampUs, QnVideoCameraPtr camera) const
{
    NX_VERBOSE(kLogTag) << lm("%1()").arg(__func__);

    auto gopFrames = getLiveCacheGopTillTime(usePrimaryStream, timestampUs, camera);
    if (!gopFrames)
        return CLVideoDecoderOutputPtr();

    if (auto decodedFrame = decodeFrameSequence(gopFrames, timestampUs))
        return decodedFrame;

    NX_VERBOSE(kLogTag) << lm("%1(): WARNING: liveCache decoding failed");
    return CLVideoDecoderOutputPtr();
}

CLVideoDecoderOutputPtr QnGetImageHelper::getImage(const nx::api::CameraImageRequest& request) const
{
    NX_VERBOSE(kLogTag) << lm("%1(%2 us, roundMethod: %3, size: %4) BEGIN")
        .args(__func__, request.usecSinceEpoch, request.roundMethod, request.size);

    if (!request.camera)
    {
        NX_VERBOSE(kLogTag) << lm("%1() END -> null: No camera").args(__func__);
        return CLVideoDecoderOutputPtr();
    }

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

    const bool usePrimaryStream = !(
        (dstSize.width() > 0 && dstSize.width() <= 480)
        || (dstSize.height() > 0 && dstSize.height() <= 316));
    if (dstSize != request.size)
        NX_VERBOSE(kLogTag) << lm("%1(): dstSize: %2").args(__func__, dstSize);

    if (auto frame = getImageWithCertainQuality(usePrimaryStream, request))
    {
        NX_VERBOSE(kLogTag) << lm("%1() END -> frame").arg(__func__);
        return frame;
    }

    // Attempting alternate stream if could not find the frame.
    if (auto frame = getImageWithCertainQuality(!usePrimaryStream, request))
    {
        NX_VERBOSE(kLogTag) << lm("%1() END -> frame from alternate stream").arg(__func__);
        return frame;
    }

    NX_VERBOSE(kLogTag) << lm("%1() END -> null").arg(__func__);
    return CLVideoDecoderOutputPtr();
}

/**
 * @return Sequence from an I-frame to the desired frame. Can be null but not empty.
 */
std::unique_ptr<QnConstDataPacketQueue> QnGetImageHelper::getLiveCacheGopTillTime(
    bool usePrimaryStream, qint64 timestampUs, QnVideoCameraPtr camera) const
{
    const MediaQuality stream = usePrimaryStream ? MEDIA_Quality_High : MEDIA_Quality_Low;
    if (!camera->liveCache(stream))
    {
        NX_VERBOSE(kLogTag) << lm("%1(): NOTE: liveCache not initialized for %2 stream")
            .args(__func__, usePrimaryStream ? "primary" : "secondary");
        return nullptr;
    }

    quint64 iFrameTimestampUs;
    QnAbstractDataPacketPtr iFrameData = camera->liveCache(stream)->findByTimestamp(
        timestampUs, /*findKeyFramesOnly*/ true, &iFrameTimestampUs);
    if (!iFrameData)
        return nullptr;
    auto iFrame = std::dynamic_pointer_cast<const QnCompressedVideoData>(iFrameData);
    if (!iFrame)
    {
        NX_VERBOSE(kLogTag) << lm("%1(): WARNING: Wrong liveCache I-frame data for %2 us: %3")
            .args(__func__, iFrameTimestampUs, iFrameData);
        return nullptr;
    }

    NX_VERBOSE(kLogTag) << lm("%1(): I-frame found: %2 us").args(__func__, iFrameTimestampUs);

    auto frames = std::make_unique<QnConstDataPacketQueue>();
    frames->push(iFrame);

    if (iFrameTimestampUs != timestampUs)
    {
        // Add subsequent P-frames.
        quint64 frameTimestampUs = iFrameTimestampUs;

        static const int kMaxPFrames = 1000;
        int i = 0;
        for (i = 0; i < kMaxPFrames; ++i)
        {
            quint64 pFrameTimestampUs;
            QnAbstractDataPacketPtr pFrameData = camera->liveCache(stream)->getNextPacket(
                frameTimestampUs, &pFrameTimestampUs);
            if (!pFrameData)
                break;
            if ((qint64) pFrameTimestampUs > timestampUs)
                break;

            frameTimestampUs = pFrameTimestampUs; //< Prepare for the next iteration.

            auto pFrame = std::dynamic_pointer_cast<const QnCompressedVideoData>(pFrameData);
            if (!pFrame)
            {
                NX_VERBOSE(kLogTag) << lm("%1(): WARNING: Wrong liveCache P-frame data for %2 us: %3")
                    .args(__func__, pFrameTimestampUs, pFrameData);
                continue;
            }

            NX_VERBOSE(kLogTag) << lm("%1(): P-frame found: %2 us").args(__func__, pFrameTimestampUs);
            frames->push(pFrame);
        }
        if (i >= kMaxPFrames)
            NX_VERBOSE(kLogTag) << lm("%1(): WARNING: Too many P-frames: %2").args(__func__, i);
    }

    return frames;
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

QByteArray QnGetImageHelper::encodeImage(const CLVideoDecoderOutputPtr& outFrame, const QByteArray& format) const
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

CLVideoDecoderOutputPtr QnGetImageHelper::getImageWithCertainQuality(
    bool usePrimaryStream, const nx::api::CameraImageRequest& request) const
{
    NX_VERBOSE(kLogTag) << lm("%1(%2, %3 us, roundMethod: %4) BEGIN").args(__func__,
        usePrimaryStream ? "primary" : "secondary", request.usecSinceEpoch, request.roundMethod);

    const auto camera = request.camera;

    QSize dstSize = request.size;

    int rotation = request.rotation;
    if (rotation == nx::api::ImageRequest::kDefaultRotation)
        rotation = camera->getProperty(QnMediaResource::rotationKey()).toInt();

    QnConstResourceVideoLayoutPtr layout = camera->getVideoLayout();

    std::unique_ptr<QnAbstractArchiveDelegate> archiveDelegate(camera->createArchiveDelegate());
    if (!archiveDelegate)
        archiveDelegate.reset(new QnServerArchiveDelegate(m_serverModule)); // default value
    archiveDelegate->setPlaybackMode(PlaybackMode::ThumbNails);
    bool isOpened = false;

    if (!usePrimaryStream)
        archiveDelegate->setQuality(MEDIA_Quality_Low, true, QSize());

    QList<QnAbstractImageFilterPtr> filterChain;

    CLVideoDecoderOutputPtr outFrame;
    int channelMask = (1 << layout->channelCount()) - 1;
    bool gotNullFrame = false;
    for (int i = 0; i < layout->channelCount(); ++i)
    {
        CLVideoDecoderOutputPtr frame = readFrame(
            request, usePrimaryStream, archiveDelegate.get(), i, isOpened);
        if (!frame)
        {
            gotNullFrame = true;
            if (i == 0)
            {
                NX_VERBOSE(kLogTag) << lm("%1() END -> null: frame not found").args(__func__);
                return CLVideoDecoderOutputPtr();
            }
            else
            {
                continue;
            }
        }
        channelMask &= ~(1 << frame->channel);
        if (i == 0)
        {
            dstSize = updateDstSize(camera, dstSize, frame, request.aspectRatio);
            if (dstSize.width() <= 16 || dstSize.height() <= 8)
            {
                NX_VERBOSE(kLogTag) << lm("%1() END -> null: frame too small").args(__func__);
                return CLVideoDecoderOutputPtr();
            }
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
                request, usePrimaryStream, archiveDelegate.get(), 0, isOpened);
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
    NX_VERBOSE(kLogTag) << lm("%1() END -> frame").args(__func__);
    return outFrame;
}

CLVideoDecoderOutputPtr QnGetImageHelper::decodeFrameSequence(
    std::unique_ptr<QnConstDataPacketQueue>& sequence, quint64 timestampUs) const
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
    for (int i = 0; i < randomAccess.size(); ++i)
    {
        auto frame = std::dynamic_pointer_cast<const QnCompressedVideoData>(randomAccess.at(i));
        gotFrame = decoder.decode(frame, &outFrame);

        NX_VERBOSE(kLogTag) << lm("%1(): Decoded: gotFrame: %2, frame->timestamp: %3, timestampUs: %4")
            .args(__func__, gotFrame ? "true" : "false", frame->timestamp, timestampUs);
        if (frame->timestamp >= (qint64) timestampUs)
            break;
    }
    while (decoder.decode(QnConstCompressedVideoDataPtr(), &outFrame))
    {
        NX_VERBOSE(kLogTag) << lm("%1(): Flushed: outFrame->pkt_dts: %2")
            .args(__func__, outFrame->pkt_dts);

        gotFrame = true; //< flush decoder buffer
    }
    return gotFrame ? outFrame : CLVideoDecoderOutputPtr();
}
