// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "archive_frame_extractor.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <thread>

#include <core/resource/avi/avi_archive_delegate.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_resource.h>
#include <decoders/video/ffmpeg_video_decoder.h>
#include <nx/media/media_data_packet.h>
#include <nx/streaming/abstract_archive_delegate.h>
#include <nx/streaming/rtsp_client_archive_delegate.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/system_context.h>
#include <utils/common/delayed.h>
#include <utils/media/frame_info.h>

using namespace std::chrono;

namespace nx::vms::client::desktop {

namespace {

static constexpr auto kIdleStreamKeepAliveTime = 15s;
static constexpr auto kAcceptableTimeDivergence = 0.1;

milliseconds frameTimeMs(const QnCompressedVideoDataPtr& encodedFrame)
{
    if (!NX_ASSERT(encodedFrame))
        return 0ms;

    return round<milliseconds>(
        microseconds(encodedFrame->timestamp));
}

bool timeSatisfiesRequest(
    const ArchiveFrameExtractor::Request& request,
    milliseconds timePoint)
{
    return timePoint >= request.timePoint - request.tolerance
        && timePoint < request.timePoint + request.tolerance;
}

double requestedTimeDivergence(
    const ArchiveFrameExtractor::Request& request,
    milliseconds timePoint)
{
    if (request.tolerance == 0ms)
        return 0.0;
    return std::fabs(request.timePoint.count() - timePoint.count())
        / request.tolerance.count();
}

bool isKeyFrame(const QnCompressedVideoDataPtr& encodedFrame)
{
    return encodedFrame->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey);
}

void setDecodeTwiceFlag(const QnCompressedVideoDataPtr& encodedFrame)
{
    encodedFrame->flags.setFlag(QnAbstractMediaData::MediaFlags_DecodeTwice);
}

} // namespace

struct ArchiveFrameExtractor::Private
{
    ArchiveFrameExtractor* const q;

    std::atomic_bool stop = false;

    struct StreamWorker
    {
        std::mutex mutex;
        std::condition_variable cv;
        std::unique_ptr<std::thread> thread;

        std::deque<Request> requestQueue;

        std::unique_ptr<QnAbstractArchiveDelegate> archiveDelegate;
        QnCompressedVideoDataPtr currentFrame;

        std::optional<microseconds> lastKeyFrameTime;
        std::optional<milliseconds> lastSeekTime;
        std::optional<microseconds> lastGopDuration;
    };
    StreamWorker streamWorker;

    struct DecodeJob
    {
        Request request;
        QSize scaleSize;
        std::vector<QnCompressedVideoDataPtr> frameSequence;
    };

    struct DecodeWorker
    {
        mutable std::mutex mutex;
        std::condition_variable cv;
        std::unique_ptr<std::thread> thread;

        std::deque<DecodeJob> decodeQueue;

        std::unique_ptr<QnFfmpegVideoDecoder> decoder;
        QSize scaleSize;
    };
    DecodeWorker decodeWorker;

    void returnResult(
        const Request& request,
        const CLVideoDecoderOutputPtr& decodedFrame,
        ResultCode resultCode)
    {
        Result result;
        result.request = request;
        result.decodedFrame = decodedFrame;
        result.resultCode = resultCode;
        executeLater([this, result]() { emit q->frameRequestDone(result); }, q);
    }

    void processRequests()
    {
        while (true)
        {
            std::unique_lock<std::mutex> lock(streamWorker.mutex);
            streamWorker.cv.wait_for(lock, kIdleStreamKeepAliveTime,
                [this] { return !streamWorker.requestQueue.empty() || stop; });

            if (stop)
                return;

            if (streamWorker.requestQueue.empty())
            {
                const auto canSeekImmediatly = streamWorker.archiveDelegate->getFlags().testFlag(
                    QnAbstractArchiveDelegate::Flag_CanSeekImmediatly);

                if (canSeekImmediatly && dynamic_cast<QnRtspClientArchiveDelegate*>(
                    streamWorker.archiveDelegate.get()))
                {
                    streamWorker.archiveDelegate->close();
                }

                lock.unlock();
                continue;
            }

            auto request = streamWorker.requestQueue.front();
            streamWorker.requestQueue.pop_front();
            lock.unlock();

            processRequest(request);
        }
    };

    void processRequest(const Request& request)
    {
        if (streamWorker.lastSeekTime && streamWorker.lastKeyFrameTime
            && request.timePoint > streamWorker.lastSeekTime.value()
            && (streamWorker.lastKeyFrameTime.value() > request.timePoint + request.tolerance))
        {
            returnResult(request, {}, ResultCode::noData);
            return;
        }

        bool seekIFrame = true;

        if (streamWorker.currentFrame)
        {
            const auto currentFrameTime = frameTimeMs(streamWorker.currentFrame);
            if (request.timePoint > currentFrameTime)
            {
                if ((currentFrameTime - streamWorker.lastKeyFrameTime.value())
                    > (request.timePoint - currentFrameTime))
                {
                    seekIFrame = false;
                }

                if (streamWorker.lastGopDuration
                    && (request.timePoint - streamWorker.lastKeyFrameTime.value())
                        > streamWorker.lastGopDuration.value())
                {
                    seekIFrame = true;
                }
            }
        }

        if (seekIFrame)
        {
            streamWorker.lastSeekTime = request.timePoint;
            streamWorker.archiveDelegate->seek(
                duration_cast<microseconds>(request.timePoint).count(),
                /*findIFrame*/ true);

            streamWorker.currentFrame = getNextVideoData();
            if (!streamWorker.currentFrame)
            {
                returnResult(request, {}, ResultCode::endReached);
                streamWorker.lastKeyFrameTime.reset();
                return;
            }
            streamWorker.lastKeyFrameTime =
                microseconds(streamWorker.currentFrame->timestamp);
        }
        else
        {
            streamWorker.currentFrame = getNextVideoData();
            if (!streamWorker.currentFrame)
            {
                returnResult(request, {}, ResultCode::endReached);
                streamWorker.lastKeyFrameTime.reset();
                return;
            }
            else
            {
                if (isKeyFrame(streamWorker.currentFrame))
                {
                    streamWorker.lastKeyFrameTime =
                        microseconds(streamWorker.currentFrame->timestamp);
                }
            }
        }

        if (streamWorker.lastKeyFrameTime.value() > request.timePoint + request.tolerance)
        {
            returnResult(request, {}, ResultCode::noData);
            return;
        }

        DecodeJob decodeJob({request});

        if (streamWorker.currentFrame)
            decodeJob.frameSequence.push_back(streamWorker.currentFrame);

        while (streamWorker.currentFrame
            && requestedTimeDivergence(request, frameTimeMs(streamWorker.currentFrame))
                > kAcceptableTimeDivergence
            && frameTimeMs(streamWorker.currentFrame) <= request.timePoint)
        {
            streamWorker.currentFrame = getNextVideoData();
            if (streamWorker.currentFrame)
            {
                if (isKeyFrame(streamWorker.currentFrame))
                {
                    streamWorker.lastGopDuration =
                        microseconds(streamWorker.currentFrame->timestamp)
                            - streamWorker.lastKeyFrameTime.value();
                    decodeJob.frameSequence.clear();
                    streamWorker.lastKeyFrameTime =
                        microseconds(streamWorker.currentFrame->timestamp);
                }
                decodeJob.frameSequence.push_back(streamWorker.currentFrame);
            }
            else
            {
                streamWorker.lastKeyFrameTime.reset();
            }
        }

        if (decodeJob.frameSequence.empty())
            return;

        {
            std::lock_guard<std::mutex> lock(decodeWorker.mutex);
            decodeJob.scaleSize = decodeWorker.scaleSize;
            decodeWorker.decodeQueue.push_back(decodeJob);
        }
        decodeWorker.cv.notify_one();
    }

    QnCompressedVideoDataPtr getNextVideoData()
    {
        QnCompressedVideoDataPtr compressedVideoData;
        while (!compressedVideoData)
        {
            auto mediaData = streamWorker.archiveDelegate->getNextData();
            if (!mediaData || mediaData->dataType == QnAbstractMediaData::EMPTY_DATA)
                return {}; //< End found.

            if (auto metaData = std::dynamic_pointer_cast<QnCompressedMetadata>(mediaData))
            {
                if (metaData->metadataType == MetadataType::MediaStreamEvent)
                    return {};
            }

            compressedVideoData = std::dynamic_pointer_cast<QnCompressedVideoData>(mediaData);
        }
        return compressedVideoData;
    };

    void decodeFrame()
    {
        while (true)
        {
            std::unique_lock<std::mutex> lock(decodeWorker.mutex);
            decodeWorker.cv.wait(lock, [this] { return !decodeWorker.decodeQueue.empty() || stop; });

            if (stop)
                return;

            auto decodeJob = decodeWorker.decodeQueue.front();
            decodeWorker.decodeQueue.pop_front();

            lock.unlock();

            if (decodeWorker.decoder && isKeyFrame(decodeJob.frameSequence.front()))
            {
                setDecodeTwiceFlag(decodeJob.frameSequence.front());
                decodeWorker.decoder->resetDecoder(decodeJob.frameSequence.front());
            }

            if (!decodeWorker.decoder)
            {
                decodeWorker.decoder = std::make_unique<QnFfmpegVideoDecoder>(
                    DecoderConfig(), /*metrics*/ nullptr, decodeJob.frameSequence.front());
                setDecodeTwiceFlag(decodeJob.frameSequence.front());
            }

            CLVideoDecoderOutputPtr currentDecodedFrame(new CLVideoDecoderOutput);
            CLVideoDecoderOutputPtr outputDecodedFrame(new CLVideoDecoderOutput);
            for (const auto& compressedData: decodeJob.frameSequence)
            {
                if (stop.load())
                    return;

                if (decodeWorker.decoder->decode(compressedData, &currentDecodedFrame))
                    std::swap(outputDecodedFrame, currentDecodedFrame);
            }

            if (outputDecodedFrame && !decodeJob.scaleSize.isEmpty())
                outputDecodedFrame = outputDecodedFrame->scaled(decodeJob.scaleSize);

            returnResult(decodeJob.request, outputDecodedFrame, outputDecodedFrame.isNull()
                ? ResultCode::decodeFailed
                : ResultCode::decoded);
        }
    };
};

ArchiveFrameExtractor::ArchiveFrameExtractor(
    const QnMediaResourcePtr& mediaResource,
    VideoQuality videQuality,
    bool sleepIfEmptySocket)
    :
    base_type(),
    d(new Private{this})
{
    if (!NX_ASSERT(!mediaResource.isNull()))
        return;

    if (!NX_ASSERT(mediaResource->hasVideo()))
        return;

    if (const auto cameraResource = mediaResource.dynamicCast<QnVirtualCameraResource>())
    {
        auto context = SystemContext::fromResource(cameraResource);
        if (!NX_ASSERT(context))
            return;

        nx::network::http::Credentials credentials;
        if (auto connection = context->connection())
            credentials = context->connection()->credentials();

        auto rtspDelegate = std::make_unique<QnRtspClientArchiveDelegate>(
            /*archiveStreamReader*/ nullptr,
            std::move(credentials),
            /*rtpLogTag*/ QString(),
            sleepIfEmptySocket);
        rtspDelegate->setCamera(cameraResource);
        rtspDelegate->setMediaRole(PlaybackMode::archive);
        d->streamWorker.archiveDelegate = std::move(rtspDelegate);
    }
    else if (const auto aviResource = mediaResource.dynamicCast<QnAviResource>())
    {
        d->streamWorker.archiveDelegate.reset(aviResource->createArchiveDelegate());
    }

    static constexpr auto kAutoSize = QSize(-1, 0);
    const MediaQuality mediaQuality = videQuality == VideoQuality::low
        ? MEDIA_Quality_Low
        : MEDIA_Quality_High;
    d->streamWorker.archiveDelegate->setQuality(mediaQuality, /*fastSwitch*/ false, kAutoSize);

    if (!d->streamWorker.archiveDelegate->getFlags().testFlag(
        QnAbstractArchiveDelegate::Flag_CanSeekImmediatly))
    {
        d->streamWorker.archiveDelegate->open(mediaResource->toResourcePtr());
    }

    d->streamWorker.thread = std::make_unique<std::thread>(&Private::processRequests, d.get());
    d->decodeWorker.thread = std::make_unique<std::thread>(&Private::decodeFrame, d.get());
}

ArchiveFrameExtractor::~ArchiveFrameExtractor()
{
    d->stop.store(true);

    d->streamWorker.cv.notify_one();
    d->decodeWorker.cv.notify_one();

    // TODO: #vbreus There is still vanishingly small probability of stream reopen due possible
    // race condition, however this won't lead to unrecoverable erroneous state. To be fixed in
    // the 5.1.
    d->streamWorker.archiveDelegate->pleaseStop();

    if (d->streamWorker.thread)
        d->streamWorker.thread->join();

    if (d->decodeWorker.thread)
        d->decodeWorker.thread->join();
}

QSize ArchiveFrameExtractor::scaleSize()
{
    std::lock_guard<std::mutex> lock(d->decodeWorker.mutex);
    return d->decodeWorker.scaleSize;
}

void ArchiveFrameExtractor::setScaleSize(const QSize& size)
{
    std::lock_guard<std::mutex> lock(d->streamWorker.mutex);
    d->decodeWorker.scaleSize = size;
}

void ArchiveFrameExtractor::requestFrame(const Request& request)
{
    {
        std::lock_guard<std::mutex> lock(d->streamWorker.mutex);
        d->streamWorker.requestQueue.push_back(request);
    }
    d->streamWorker.cv.notify_one();
}

void ArchiveFrameExtractor::clearRequestQueue()
{
    std::scoped_lock lock{d->streamWorker.mutex, d->decodeWorker.mutex};
    d->streamWorker.requestQueue.clear();
    d->decodeWorker.decodeQueue.clear();
}

} // namespace nx::vms::client::desktop
