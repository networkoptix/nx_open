#include "get_image_helper.h"

#include <camera/camera_pool.h>
#include <camera/video_camera.h>

#include <core/resource/camera_resource.h>

#include <mediaserver_ini.h>

#include "utils/media/frame_info.h"
#include "transcoding/transcoder.h"
#include "transcoding/filters/tiled_image_filter.h"
#include "transcoding/filters/scale_image_filter.h"
#include "transcoding/filters/rotate_image_filter.h"


#include "plugins/resource/server_archive/server_archive_delegate.h"
#include <decoders/video/ffmpeg_video_decoder.h>
#include <nx/utils/log/log_main.h>
#include "media_server/media_server_module.h"

using StreamIndex = nx::vms::api::StreamIndex;

namespace {

static constexpr int kMaxGopLen = 100;

static constexpr int kRoundFactor = 4;

static StreamIndex oppositeStreamIndex(StreamIndex streamIndex)
{
    switch (streamIndex)
    {
        case StreamIndex::primary:
            return StreamIndex::secondary;
        case StreamIndex::secondary:
            return StreamIndex::primary;
        default:
            break;
    }
    NX_ASSERT(false, lm("Unsupported StreamIndex %1").args(streamIndex));
    return StreamIndex::undefined; //< Fallback for the failed assertion.
}

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
    nx::vms::server::ServerModuleAware(serverModule)
{
}

QSize updateDstSize(
    const QnVirtualCameraResource* camera,
    const QSize& srcSize,
    const CLVideoDecoderOutput& outFrame,
    nx::api::ImageRequest::AspectRatio aspectRatio)
{
    QSize dstSize(srcSize);

    const double sar = outFrame.sample_aspect_ratio;
    const double ar = sar * outFrame.width / outFrame.height;
    NX_ASSERT(ar > 0);
    if (!dstSize.isEmpty())
    {
        dstSize.setHeight(qPower2Ceil((unsigned)dstSize.height(), kRoundFactor));
        dstSize.setWidth(qPower2Ceil((unsigned)dstSize.width(), kRoundFactor));
    }
    else if (dstSize.height() > 0)
    {
        dstSize.setHeight(qPower2Ceil((unsigned)dstSize.height(), kRoundFactor));
        dstSize.setWidth(qPower2Ceil((unsigned)(dstSize.height() * ar), kRoundFactor));
    }
    else if (dstSize.width() > 0)
    {
        dstSize.setWidth(qPower2Ceil((unsigned)dstSize.width(), kRoundFactor));
        dstSize.setHeight(qPower2Ceil((unsigned)(dstSize.width() / ar), kRoundFactor));
    }
    else
    {
        dstSize = QSize(outFrame.width * sar, outFrame.height);
    }

    // If auto was requested, then should use aspect ratio like it is used in GUI.
    if (aspectRatio == nx::api::ImageRequest::AspectRatio::auto_)
    {
        const auto customAr = camera->aspectRatio();
        if (customAr.isValid())
            dstSize.setWidth(dstSize.height() * customAr.toFloat());
    }

    // Scale the image to fit inside max size square.
    static constexpr int kMaxSize = nx::api::CameraImageRequest::kMaximumSize;
    if (dstSize.width() > kMaxSize || dstSize.height() > kMaxSize)
        dstSize.scale(kMaxSize, kMaxSize, Qt::KeepAspectRatio);

    // Scale the image to fit outside min size square.
    static constexpr int kMinSize = nx::api::CameraImageRequest::kMinimumSize;
    if ((dstSize.height() < kMinSize && dstSize.height() > 0)
        || (dstSize.width() < kMinSize && dstSize.width() > 0))
        dstSize.scale(kMinSize, kMinSize, Qt::KeepAspectRatioByExpanding);

    dstSize = QnCodecTranscoder::roundSize(dstSize);
    NX_VERBOSE(typeid(QnGetImageHelper), "%1(): dstSize: %2", __func__, dstSize);
    return dstSize;
}

CLVideoDecoderOutputPtr QnGetImageHelper::readFrame(
    const nx::api::CameraImageRequest& request,
    StreamIndex streamIndex,
    QnAbstractArchiveDelegate* archiveDelegate,
    int preferredChannel,
    bool& isOpened) const
{
    if (!NX_ASSERT(streamIndex == StreamIndex::primary
        || streamIndex == StreamIndex::secondary))
    {
        return nullptr;
    }

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

    auto camera = serverModule()->videoCameraPool()->getVideoCamera(resource);

    QnConstCompressedVideoDataPtr video;
    bool isArchiveVideoPacket = false;
    if (request.usecSinceEpoch == DATETIME_NOW)
    {
        if (camera)
        {
            video = camera->getLastVideoFrame(streamIndex, preferredChannel);
            if (video)
                NX_VERBOSE(this, "%1(NOW): Got camera last frame", __func__);
            else
                NX_VERBOSE(this, "%1(NOW): Reject: camera last frame missing", __func__);
        }
        else
        {
            NX_VERBOSE(this, "%1(NOW): Reject: camera missing", __func__);
        }
    }
    else if (request.usecSinceEpoch == nx::api::ImageRequest::kLatestThumbnail)
    {
        NX_VERBOSE(this, "%1(LATEST): Get latest data", __func__);
        if (camera)
        {
            video = camera->getLastVideoFrame(streamIndex, preferredChannel);
            if (!video)
            {
                video = camera->getLastVideoFrame(
                    oppositeStreamIndex(streamIndex), preferredChannel);
                if (video)
                {
                    NX_VERBOSE(this, "%1(LATEST): Got camera last frame for the alternate stream",
                        __func__);
                }
                else
                {
                    NX_VERBOSE(this, "%1(LATEST): Missing camera last frame for both streams",
                        __func__);
                }
            }
            else
            {
                NX_VERBOSE(this, "%1(LATEST): Got camera last frame for the requested stream",
                    __func__);
            }
        }
        // Don't look at archive on slow devices (dts based) if noArchiveOnSlowDevices is enabled.
        if (!video && !(resource->isDtsBased() && request.ignoreExternalArchive))
        {

            openDelegateIfNeeded([&]() { return archiveDelegate->endTime() - 1000 * 100; });
            video = getNextArchiveVideoPacket(archiveDelegate, AV_NOPTS_VALUE);
            if (video)
                NX_VERBOSE(this, "%1(LATEST): Got from archive", __func__);
            else
                NX_VERBOSE(this, "%1(LATEST): Missing from archive", __func__);
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
            NX_VERBOSE(this, "%1(%2 us): Got from archive", __func__, timestampUs);
        else
            NX_VERBOSE(this, "%1(%2 us): Missing from archive", __func__, timestampUs);

        if (!video && camera)
        {
            return decodeFrameFromCaches(
                camera, streamIndex, timestampUs, preferredChannel, request.roundMethod);
        }
    }

    if (!video)
        return nullptr;

    CLVideoDecoderOutputPtr outFrame(new CLVideoDecoderOutput());
    bool gotFrame = false;
    QnFfmpegVideoDecoder decoder(
        DecoderConfig::fromResource(resource), video->compressionType, video, false);

    if (!isArchiveVideoPacket)
    {
        if (resource->isOnline() || request.usecSinceEpoch != DATETIME_NOW)
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

    return gotFrame ? outFrame : nullptr;
}

CLVideoDecoderOutputPtr QnGetImageHelper::decodeFrameFromCaches(
    QnVideoCameraPtr camera,
    StreamIndex streamIndex,
    qint64 timestampUs,
    int preferredChannel,
    nx::api::ImageRequest::RoundMethod roundMethod) const
{
    const auto logPrefix = lm("%1(%2, %3 us, channel: %4, %5):").args(__func__,
        streamIndex, timestampUs, preferredChannel, roundMethod);

    // Try GopKeeper.
    auto videoSequence = camera->getFrameSequenceByTime(
        streamIndex, timestampUs, preferredChannel, roundMethod);
    if (videoSequence)
    {
        NX_VERBOSE(this, "%1 Got from GopKeeper", logPrefix);
        return decodeFrameSequence(camera->resource(), videoSequence, timestampUs);
    }

    // Try liveCache.
    if (auto frame = decodeFrameFromLiveCache(streamIndex, timestampUs, camera))
    {
        NX_VERBOSE(this, "%1 Got from liveCache: %2 us", logPrefix, frame->pkt_dts);
        return frame;
    }

    NX_VERBOSE(this, "%1 Missing from GopKeeper for both streams and liveCache", logPrefix);
    return nullptr;
}

CLVideoDecoderOutputPtr QnGetImageHelper::decodeFrameFromLiveCache(
    StreamIndex streamIndex, qint64 timestampUs, QnVideoCameraPtr camera) const
{
    NX_VERBOSE(this, "%1()", __func__);

    auto gopFrames = getLiveCacheGopTillTime(streamIndex, timestampUs, camera);
    if (!gopFrames)
        return nullptr;

    if (auto decodedFrame = decodeFrameSequence(camera->resource(), gopFrames, timestampUs))
        return decodedFrame;

    NX_VERBOSE(this, "%1(): WARNING: liveCache decoding failed", __func__);
    return nullptr;
}

CLVideoDecoderOutputPtr QnGetImageHelper::getImage(const nx::api::CameraImageRequest& request) const
{
    NX_VERBOSE(this, "%1(%2 us, roundMethod: %3, size: %4) BEGIN",
        __func__, request.usecSinceEpoch, request.roundMethod, request.size);

    if (!request.camera)
    {
        NX_VERBOSE(this, "%1() END -> null: No camera", __func__);
        return nullptr;
    }

    const auto streamIndex = determineStreamIndex(request);

    if (auto frame = getImageWithCertainQuality(streamIndex, request))
    {
        NX_VERBOSE(this, "%1() END -> frame", __func__);
        return frame;
    }

    // Attempting alternate stream if could not find the frame.
    if (auto frame = getImageWithCertainQuality(oppositeStreamIndex(streamIndex), request))
    {
        NX_VERBOSE(this, "%1() END -> frame from alternate stream", __func__);
        return frame;
    }

    NX_VERBOSE(this, "%1() END -> null", __func__);
    return nullptr;
}

/**
 * @return Sequence from an I-frame to the desired frame. Can be null but not empty.
 */
std::unique_ptr<QnConstDataPacketQueue> QnGetImageHelper::getLiveCacheGopTillTime(
    StreamIndex streamIndex, qint64 timestampUs, QnVideoCameraPtr camera) const
{
    const MediaQuality stream = (streamIndex == StreamIndex::primary)
        ? MEDIA_Quality_High
        : MEDIA_Quality_Low;
    if (!camera->liveCache(stream))
    {
        NX_VERBOSE(this, "%1(): NOTE: liveCache not initialized for %2 stream",
            __func__, streamIndex);
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
        NX_VERBOSE(this, "%1(): WARNING: Wrong liveCache I-frame data for %2 us: %3",
            __func__, iFrameTimestampUs, iFrameData);
        return nullptr;
    }

    NX_VERBOSE(this, "%1(): I-frame found: %2 us", __func__, iFrameTimestampUs);

    auto frames = std::make_unique<QnConstDataPacketQueue>();
    frames->push(iFrame);

    if (iFrameTimestampUs != timestampUs)
    {
        // Add subsequent P-frames.
        quint64 frameTimestampUs = iFrameTimestampUs;

        int i = 0;
        for (i = 0; i < kMaxGopLen; ++i)
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
                NX_VERBOSE(this, "%1(): WARNING: Wrong liveCache P-frame data for %2 us: %3",
                    __func__, pFrameTimestampUs, pFrameData);
                continue;
            }

            NX_VERBOSE(this, "%1(): P-frame found: %2 us", __func__, pFrameTimestampUs);
            frames->push(pFrame);
        }
        if (i >= kMaxGopLen)
            NX_VERBOSE(this, "%1(): WARNING: Too many P-frames: %2", __func__, i);
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
        NX_WARNING(this, "Can't initialize ffmpeg encoder to encode image. "
            "codec=%1, pixel format=%2, size=%3x%4",
            codecId, pixelFormat, outFrame->width, outFrame->height);
    }
    else
    {
        QnFfmpegAvPacket outPacket;
        int got_packet = 0;
        int encodeResult = avcodec_encode_video2(
            videoEncoderCodecCtx, &outPacket, outFrame.data(), &got_packet);

        if (encodeResult == 0 && got_packet)
        {
            result.append((const char*) outPacket.data, outPacket.size);
        }
        else
        {
            NX_WARNING(this, "Can't encode image. codec=%1, pixel format=%2, size=%3x%4, errCode=%5",
                codecId, pixelFormat, outFrame->width, outFrame->height, encodeResult);
        }

    }

    QnFfmpegHelper::deleteAvCodecContext(videoEncoderCodecCtx);
    return result;
}

StreamIndex QnGetImageHelper::determineStreamIndex(
    const nx::api::CameraImageRequest &request) const
{
    NX_VERBOSE(this, "%1(%2)", __func__, request.streamSelectionMode);

    using StreamSelectionMode = nx::api::CameraImageRequest::StreamSelectionMode;
    switch (request.streamSelectionMode)
    {
        case StreamSelectionMode::auto_:
        {
            #if defined(EDGE_SERVER)
                // On edge, we always try to use the secondary stream first.
                return StreamIndex::secondary;
            #endif

            const auto secondaryResolution =
                request.camera->streamInfo(StreamIndex::secondary).getResolution();
            if ((request.size.width() <= 0 && request.size.height() <= 0)
                || request.size.width() > secondaryResolution.width()
                || request.size.height() > secondaryResolution.height())
            {
                return StreamIndex::primary;
            }

            return StreamIndex::secondary;
        }
        case StreamSelectionMode::forcedPrimary: return StreamIndex::primary;
        case StreamSelectionMode::forcedSecondary: return StreamIndex::secondary;
        case StreamSelectionMode::sameAsAnalytics:
            return ini().analyzeSecondaryStream ? StreamIndex::secondary : StreamIndex::primary;
        case StreamSelectionMode::sameAsMotion:
            return request.camera->motionStreamIndex().index;
    }

    NX_ASSERT(false);
    return StreamIndex::undefined;
}

CLVideoDecoderOutputPtr QnGetImageHelper::getImageWithCertainQuality(
    StreamIndex streamIndex, const nx::api::CameraImageRequest& request) const
{
    NX_VERBOSE(this, "%1(%2, %3 us, roundMethod: %4) BEGIN",
        __func__, streamIndex, request.usecSinceEpoch, request.roundMethod);

    const auto camera = request.camera;

    int rotation = request.rotation;
    if (rotation == nx::api::ImageRequest::kDefaultRotation)
        rotation = camera->getProperty(QnMediaResource::rotationKey()).toInt();

    QnConstResourceVideoLayoutPtr layout = camera->getVideoLayout();

    std::unique_ptr<QnAbstractArchiveDelegate> archiveDelegate(camera->createArchiveDelegate());
    if (!archiveDelegate)
        archiveDelegate.reset(new QnServerArchiveDelegate(serverModule())); // default value
    archiveDelegate->setPlaybackMode(PlaybackMode::ThumbNails);
    bool isOpened = false;

    if (streamIndex == StreamIndex::secondary)
        archiveDelegate->setQuality(MEDIA_Quality_Low, true, QSize());

    QList<QnAbstractImageFilterPtr> filterChain;

    CLVideoDecoderOutputPtr outFrame;
    int channelMask = (1 << layout->channelCount()) - 1;
    bool gotNullFrame = false;
    for (int i = 0; i < layout->channelCount(); ++i)
    {
        CLVideoDecoderOutputPtr frame = readFrame(
            request, streamIndex, archiveDelegate.get(), i, isOpened);
        if (!frame)
        {
            gotNullFrame = true;
            if (i != 0)
                continue;
            NX_VERBOSE(this, "%1() END -> null: frame not found", __func__);
            return nullptr;
        }
        channelMask &= ~(1 << frame->channel);
        if (i == 0)
        {
            const QSize dstSize = updateDstSize(camera.get(),
                request.size, *frame, request.aspectRatio);
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
                request, streamIndex, archiveDelegate.get(), 0, isOpened);
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
    NX_VERBOSE(this, "%1() END -> frame %2", __func__, outFrame->size());
    return outFrame;
}

CLVideoDecoderOutputPtr QnGetImageHelper::decodeFrameSequence(
    const QnResourcePtr& resource,
    std::unique_ptr<QnConstDataPacketQueue>& sequence, quint64 timestampUs) const
{
    if (!sequence || sequence->isEmpty())
        return nullptr;

    bool gotFrame = false;
    auto randomAccess = sequence->lock();
    auto firstFrame = std::dynamic_pointer_cast<const QnCompressedVideoData>(randomAccess.at(0));

    if (!firstFrame)
        return nullptr;

    CLVideoDecoderOutputPtr outFrame(new CLVideoDecoderOutput());
    QnFfmpegVideoDecoder decoder(
        DecoderConfig::fromResource(resource),
        firstFrame->compressionType, firstFrame, false);
    for (int i = 0; i < randomAccess.size(); ++i)
    {
        auto frame = std::dynamic_pointer_cast<const QnCompressedVideoData>(randomAccess.at(i));
        gotFrame = decoder.decode(frame, &outFrame);

        NX_VERBOSE(this, "%1(): Decoded: gotFrame: %2, frame->timestamp: %3, timestampUs: %4",
            __func__, gotFrame, frame->timestamp, timestampUs);
        if (frame->timestamp >= (qint64) timestampUs)
            break;
    }
    while (decoder.decode(QnConstCompressedVideoDataPtr(), &outFrame))
    {
        NX_VERBOSE(this, "%1(): Flushed: outFrame->pkt_dts: %2", __func__, outFrame->pkt_dts);
        gotFrame = true; //< flush decoder buffer
    }
    if (gotFrame)
        outFrame->channel = firstFrame->channelNumber;

    return gotFrame ? outFrame : nullptr;
}
