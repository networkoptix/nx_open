#include "streaming_chunk_transcoder.h"

#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/log/log.h>
#include <nx/utils/timer_manager.h>

#include <api/helpers/camera_id_helper.h>
#include <providers/filtered_ondemand_data_provider.h>
#include <media/filters/h264_mp4_to_annexb.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <recording/time_period.h>
#include <transcoding/ffmpeg_transcoder.h>

#include <camera/video_camera.h>

#include "ondemand_media_data_provider.h"
#include "live_media_cache_reader.h"
#include "streaming_chunk_cache_key.h"
#include "streaming_chunk_transcoder_thread.h"
#include <media_server/media_server_module.h>
#include <core/dataprovider/data_provider_factory.h>
#include <utils/media/av_codec_helper.h>

/**
 * Maximum time offset (in micros) requested chunk can be ahead of current live position.
 * Such chunk will be prepared as soon as live position reaches chunk start time.
 */
static const double MAX_CHUNK_TIMESTAMP_ADVANCE_MICROS = 30 * 1000 * 1000;
static const int TRANSCODE_THREAD_COUNT = 1;
static const int USEC_IN_MSEC = 1000;
static const int MSEC_IN_SEC = 1000;

StreamingChunkTranscoder::TranscodeContext::TranscodeContext():
    chunk(NULL),
    transcoder(NULL)
{
}

StreamingChunkTranscoder::StreamingChunkTranscoder(
    QnMediaServerModule* serverModule,
    Flags flags)
    :
    nx::vms::server::ServerModuleAware(serverModule),
    m_flags(flags),
    m_dataSourceCache(serverModule->timerManager())
{
    m_transcodeThreads.resize(TRANSCODE_THREAD_COUNT);
    for (size_t i = 0; i < m_transcodeThreads.size(); ++i)
    {
        m_transcodeThreads[i] = new StreamingChunkTranscoderThread();
        connect(
            m_transcodeThreads[i], &StreamingChunkTranscoderThread::transcodingFinished,
            this, &StreamingChunkTranscoder::onTranscodingFinished,
            Qt::DirectConnection);
        m_transcodeThreads[i]->start();
    }

    Qn::directConnect(
        serverModule->resourcePool(), &QnResourcePool::resourceRemoved,
        this, &StreamingChunkTranscoder::onResourceRemoved);
}

StreamingChunkTranscoder::~StreamingChunkTranscoder()
{
    stop();
}

void StreamingChunkTranscoder::stop()
{
    directDisconnectAll();

    // Cancelling all scheduled transcodings.
    {
        QnMutexLocker lk(&m_mutex);
        m_terminated = true;
    }

    for (auto val: m_taskIDToTranscode)
        serverModule()->commonModule()->timerManager()->joinAndDeleteTimer(val.first);

    std::for_each(
        m_transcodeThreads.begin(),
        m_transcodeThreads.end(),
        std::default_delete<StreamingChunkTranscoderThread>());
    m_transcodeThreads.clear();
    m_dataSourceCache.clear();
}

bool StreamingChunkTranscoder::transcodeAsync(
    const StreamingChunkCacheKey& transcodeParams,
    StreamingChunkPtr chunk)
{
    using namespace std::chrono;

    if (!validateTranscodingParameters(transcodeParams))
        return false;

    // Searching for resource.
    QnSecurityCamResourcePtr cameraResource =
        nx::camera_id_helper::findCameraByFlexibleId(
            serverModule()->resourcePool(),
            transcodeParams.srcResourceUniqueID());
    if (!cameraResource)
    {
        NX_DEBUG(this, lm("Requested resource %1 is not a media resource")
            .arg(transcodeParams.srcResourceUniqueID()));
        return false;
    }

    auto camera = videoCameraPool()->getVideoCamera(cameraResource);
    // Camera is inserted to this pool asynchronously. So it could be a race condition when
    // camera already at resourcePool but still missing at videoCameraPool().
    if (!camera)
    {
        NX_DEBUG(this, lm("Requested resource %1 is not completly registered yet")
            .arg(transcodeParams.srcResourceUniqueID()));
        return false;
    }

    // If requested stream time range is in the future for not futher than MAX_CHUNK_TIMESTAMP_ADVANCE
    // then scheduling transcoding task.

    const int newTranscodingId = m_transcodeIDSeq.fetchAndAddAcquire(1);

    chunk->openForModification();

    DataSourceContextPtr dataSourceCtx =
        prepareDataSourceContext(cameraResource, camera, transcodeParams);
    if (!dataSourceCtx)
    {
        chunk->doneModification(StreamingChunk::rcError);
        return false;
    }

    if (transcodeParams.live())
    {
        NX_DEBUG(this, lm("Starting transcoding startTimestamp %1, duration %2")
            .arg(transcodeParams.startTimestamp()).arg(transcodeParams.duration()));

        const auto liveCache = camera->liveCache(transcodeParams.streamQuality());
        if (!liveCache)
        {
            chunk->doneModification(StreamingChunk::rcError);
            return false;
        }

        const quint64 cacheEndTimestamp = liveCache->currentTimestamp();
        if (transcodeParams.alias().isEmpty() &&
            transcodeParams.startTimestamp() > cacheEndTimestamp &&
            transcodeParams.startTimestamp() - cacheEndTimestamp < MAX_CHUNK_TIMESTAMP_ADVANCE_MICROS)
        {
            // Chunk is in the future not futher than MAX_CHUNK_TIMESTAMP_ADVANCE_MICROS.
            // Scheduling transcoding on data availability.
            std::pair<std::map<int, TranscodeContext>::iterator, bool> p =
                m_scheduledTranscodings.emplace(newTranscodingId, TranscodeContext());
            NX_ASSERT(p.second);

            p.first->second.mediaResource = cameraResource;
            p.first->second.dataSourceCtx = dataSourceCtx;
            p.first->second.transcodeParams = transcodeParams;
            p.first->second.chunk = chunk;

            if (scheduleTranscoding(
                   newTranscodingId,
                   (transcodeParams.startTimestamp() - cacheEndTimestamp) / USEC_IN_MSEC + 1))
            {
                return true;
            }
            chunk->doneModification(StreamingChunk::rcError);
            m_scheduledTranscodings.erase(p.first);
            return false;
        }
    }

    if (!startTranscoding(
            newTranscodingId,
            dataSourceCtx,
            transcodeParams,
            chunk))
    {
        chunk->doneModification(StreamingChunk::rcError);
        return false;
    }

    return true;
}

DataSourceContextPtr StreamingChunkTranscoder::prepareDataSourceContext(
    QnSecurityCamResourcePtr cameraResource,
    QnVideoCameraPtr camera,
    const StreamingChunkCacheKey& transcodeParams)
{
    DataSourceContextPtr dataSourceCtx = m_dataSourceCache.take(transcodeParams);
    if (dataSourceCtx)
    {
        NX_VERBOSE(this, lm("Taking reader for resource %1, start timestamp %2, duration %3 from cache")
            .arg(transcodeParams.srcResourceUniqueID()).arg(transcodeParams.startTimestamp())
            .arg(transcodeParams.duration()));
    }
    else
    {
        dataSourceCtx = std::make_shared<DataSourceContext>(nullptr, nullptr);
    }

    if (!dataSourceCtx->mediaDataProvider)
    {
        AbstractOnDemandDataProviderPtr mediaDataProvider;
        if (transcodeParams.live())
        {
            dataSourceCtx->videoCameraLocker =
                videoCameraPool()->getVideoCameraLockerByResourceId(cameraResource->getId());
            if (!dataSourceCtx->videoCameraLocker)
            {
                NX_DEBUG(this, lm("Could not lock camera %1 (%2)")
                    .args(cameraResource->getId(), cameraResource->getName()));
                return nullptr;
            }
            mediaDataProvider = createLiveMediaDataProvider(
                *dataSourceCtx->videoCameraLocker,
                camera,
                transcodeParams);
        }
        else
        {
            mediaDataProvider = createArchiveReader(
                cameraResource, transcodeParams, QnUuid());
            //< TODO: #dmishin Pass real client id
        }

        if (!mediaDataProvider)
            return nullptr;

        dataSourceCtx->mediaDataProvider = AbstractOnDemandDataProviderPtr(
            new FilteredOnDemandDataProvider(mediaDataProvider, std::make_shared<H2645Mp4ToAnnexB>()));
    }

    if (!dataSourceCtx->transcoder)
    {
        // Creating transcoder.
        dataSourceCtx->transcoder =
            createTranscoder(cameraResource, transcodeParams);
        if (!dataSourceCtx->transcoder)
        {
            NX_WARNING(this, "Failed to create transcoder. resource %1, format %2",
                transcodeParams.srcResourceUniqueID(), transcodeParams.containerFormat());
            return nullptr;
        }
    }

    return dataSourceCtx;
}

AbstractOnDemandDataProviderPtr StreamingChunkTranscoder::createLiveMediaDataProvider(
    const VideoCameraLocker& /*locker*/,
    QnVideoCameraPtr camera,
    const StreamingChunkCacheKey& transcodeParams)
{
    using namespace std::chrono;

    const auto liveCache = camera->liveCache(transcodeParams.streamQuality());
    if (!liveCache)
        return nullptr;

    NX_VERBOSE(this, lm("Creating LIVE reader for resource %1, start timestamp %2, duration %3")
        .arg(transcodeParams.srcResourceUniqueID())
        .arg(transcodeParams.startTimestamp())
        .arg(transcodeParams.duration()));

    const quint64 cacheStartTimestamp = liveCache->startTimestamp();
    const quint64 cacheEndTimestamp = liveCache->currentTimestamp();
    const quint64 actualStartTimestamp =
        std::max<>(cacheStartTimestamp, transcodeParams.startTimestamp());
    auto mediaDataProvider = AbstractOnDemandDataProviderPtr(
        new LiveMediaCacheReader(liveCache, actualStartTimestamp));

    if ((transcodeParams.startTimestamp() < cacheEndTimestamp && //< Requested data is in live cache (at least, partially).
        transcodeParams.endTimestamp() > cacheStartTimestamp) ||
        (transcodeParams.startTimestamp() > cacheEndTimestamp && //< Chunk is in the future not futher
                                                                    //< than MAX_CHUNK_TIMESTAMP_ADVANCE_MICROS.
            transcodeParams.startTimestamp() - cacheEndTimestamp < MAX_CHUNK_TIMESTAMP_ADVANCE_MICROS) ||
        !transcodeParams.alias().isEmpty()) //< Has alias, startTimestamp may be invalid.
    {
    }
    else
    {
        // No data. E.g., request is too far away in the future.
        return nullptr;
    }

    return mediaDataProvider;
}

AbstractOnDemandDataProviderPtr StreamingChunkTranscoder::createArchiveReader(
    QnSecurityCamResourcePtr cameraResource,
    const StreamingChunkCacheKey& transcodeParams,
    const QnUuid& clientId)
{
    using namespace std::chrono;

    NX_VERBOSE(this, lm("Creating archive reader for resource %1, start timestamp %2, duration %3")
        .arg(transcodeParams.srcResourceUniqueID()).arg(transcodeParams.startTimestamp())
        .arg(transcodeParams.duration()));

    // Creating archive reader.
    QSharedPointer<QnAbstractStreamDataProvider> dp(
        serverModule()->dataProviderFactory()->createDataProvider(cameraResource, Qn::CR_Archive));
    if (!dp)
    {
        NX_WARNING(this, lm("Failed (1) to create archive data provider (resource %1)").
            arg(transcodeParams.srcResourceUniqueID()));
        return nullptr;
    }

    QnAbstractArchiveStreamReader* archiveReader =
        dynamic_cast<QnAbstractArchiveStreamReader*>(dp.data());
    archiveReader->getArchiveDelegate()->setGroupId(clientId.toByteArray());
    archiveReader->getArchiveDelegate()->setPlaybackMode(PlaybackMode::Export);
    if (!archiveReader)
    {
        NX_WARNING(this, lm("Failed (2) to create archive data provider (resource %1)").
            arg(transcodeParams.srcResourceUniqueID()));
        return nullptr;
    }
    archiveReader->setQuality(
        transcodeParams.streamQuality() == MEDIA_Quality_High
        ? MEDIA_Quality_ForceHigh
        : transcodeParams.streamQuality(),
        true);
    archiveReader->setPlaybackRange(QnTimePeriod(
        transcodeParams.startTimestamp() / USEC_IN_MSEC,
        duration_cast<milliseconds>(transcodeParams.duration()).count()));

    archiveReader->jumpTo(transcodeParams.startTimestamp(), 0);

    auto mediaDataProvider = OnDemandMediaDataProviderPtr(new OnDemandMediaDataProvider(dp));
    archiveReader->start();

    return mediaDataProvider;
}

void StreamingChunkTranscoder::onTimer(const quint64& timerID)
{
    QnMutexLocker lk(&m_mutex);

    if (m_terminated)
        return;

    std::map<quint64, int>::const_iterator taskIter = m_taskIDToTranscode.find(timerID);
    if (taskIter == m_taskIDToTranscode.end())
    {
        NX_DEBUG(this, lm("Received timer event with unknown id %1. Ignoring...").arg(timerID));
        return;
    }

    const int transcodeID = taskIter->second;
    m_taskIDToTranscode.erase(taskIter);

    std::map<int, TranscodeContext>::iterator transcodeIter =
        m_scheduledTranscodings.find(transcodeID);
    if (transcodeIter == m_scheduledTranscodings.end())
    {
        NX_DEBUG(this, lm("onTimer. "
            "Received timer event (%1) with unknown transcode id %2. Ignoring...").
            arg(timerID).arg(transcodeID));
        return;
    }

    NX_DEBUG(this, lm("onTimer. Received timer event with id %1. Resource %1").
        arg(timerID).arg(transcodeIter->second.mediaResource->toResource()->getUniqueId()));

    if (!startTranscoding(
            transcodeID,
            transcodeIter->second.dataSourceCtx,
            transcodeIter->second.transcodeParams,
            transcodeIter->second.chunk))
    {
        NX_WARNING(this, lm("onTimer. Failed to start transcoding (resource %1)").
            arg(transcodeIter->second.mediaResource->toResource()->getUniqueId()));
        transcodeIter->second.chunk->doneModification(StreamingChunk::rcError);
    }

    m_scheduledTranscodings.erase(transcodeIter);
}

bool StreamingChunkTranscoder::startTranscoding(
    int transcodingID,
    DataSourceContextPtr dataSourceCtx,
    const StreamingChunkCacheKey& transcodeParams,
    StreamingChunkPtr chunk)
{
    // Selecting least used transcoding thread from pool.
    StreamingChunkTranscoderThread* transcoderThread = *std::max_element(
        m_transcodeThreads.cbegin(),
        m_transcodeThreads.cend(),
        [](StreamingChunkTranscoderThread* one, StreamingChunkTranscoderThread* two)
        {
            return one->ongoingTranscodings() < two->ongoingTranscodings();
        });

    // Adding transcoder to transcoding thread.
    return transcoderThread->startTranscoding(
        transcodingID,
        chunk,
        dataSourceCtx,
        transcodeParams);
}

bool StreamingChunkTranscoder::scheduleTranscoding(
    const int transcodeID,
    int delayMSec)
{
    const quint64 taskID = serverModule()->commonModule()->timerManager()->addTimer(
        this,
        std::chrono::milliseconds(delayMSec));

    QnMutexLocker lk(&m_mutex);
    m_taskIDToTranscode[taskID] = transcodeID;
    return true;
}

bool StreamingChunkTranscoder::validateTranscodingParameters(
    const StreamingChunkCacheKey& /*transcodeParams*/)
{
    //TODO #ak
    return true;
}

std::unique_ptr<QnTranscoder> StreamingChunkTranscoder::createTranscoder(
    const QnSecurityCamResourcePtr& mediaResource,
    const StreamingChunkCacheKey& transcodeParams)
{
    NX_VERBOSE(this, lm("Creating new chunk transcoder for resource %1 (%2)")
        .arg(mediaResource->toResource()->getName()).arg(mediaResource->toResource()->getId()));


    QnFfmpegTranscoder::Config config;
    std::unique_ptr<QnTranscoder> transcoder(new QnFfmpegTranscoder(config, mediaResource->commonModule()->metrics()));
    if (transcoder->setContainer(transcodeParams.containerFormat()) != 0)
    {
        NX_WARNING(this, lm("Failed to create transcoder with container \"%1\" to transcode chunk (%2 - %3) of resource %4")
            .arg(transcodeParams.containerFormat()).arg(transcodeParams.startTimestamp())
            .arg(transcodeParams.endTimestamp()).arg(transcodeParams.srcResourceUniqueID()));
        return nullptr;
    }

    if (transcodeParams.audioCodecId() != AV_CODEC_ID_NONE)
    {
        if (transcoder->setAudioCodec(
            transcodeParams.audioCodecId(),
            QnTranscoder::TM_DirectStreamCopy) != 0)
        {
            NX_WARNING(this, lm("Failed to create transcoder with audio codec \"%1\" to transcode chunk (%2 - %3) of resource %4").
                arg(transcodeParams.audioCodecId()).arg(transcodeParams.startTimestamp()).
                arg(transcodeParams.endTimestamp()).arg(transcodeParams.srcResourceUniqueID()));
            return nullptr;
        }
    }

    if (!mediaResource->hasVideo())
        return transcoder;

    transcoder->setBeforeOpenCallback(
        [this, mediaResource, transcodeParams] (
            QnTranscoder* transcoder,
            const QnConstCompressedVideoDataPtr& video,
            const QnConstCompressedAudioDataPtr& /*audio*/)
        {
            if (!video)
                return;
            
            // Determine destination video codec and transcode method.
            AVCodecID dstCodecId = video->compressionType;
            bool isCodecSupported = false;
            for (const auto& supportedCodec: transcodeParams.supportedVideoCodecs())
                isCodecSupported |= (supportedCodec == dstCodecId);
            if (!isCodecSupported && !transcodeParams.supportedVideoCodecs().empty())
                dstCodecId = (AVCodecID) *transcodeParams.supportedVideoCodecs().begin();

            const QSize videoSize = QSize(video->width, video->height);
            QSize dstResolution = transcodeParams.pictureSizePixels();
            QnTranscoder::TranscodeMethod transcodeMethod = QnTranscoder::TM_DirectStreamCopy;
            if (video->compressionType != dstCodecId || videoSize != dstResolution)
            {
                transcodeMethod = QnTranscoder::TM_FfmpegTranscode;
                QnLegacyTranscodingSettings settings;
                settings.resource = mediaResource; //< Need for image filters.
                transcoder->setTranscodingSettings(settings);
            }
            
            if (transcoder->setVideoCodec(dstCodecId, transcodeMethod, Qn::StreamQuality::normal, dstResolution) != 0)
            {
                NX_WARNING(this, 
                    "Failed to create transcoder with video codec \"%1\" to transcode chunk (%2 - %3) of resource %4",
                    dstCodecId, transcodeParams.startTimestamp(),
                    transcodeParams.endTimestamp(), transcodeParams.srcResourceUniqueID());
            }
            else
            {
                NX_VERBOSE(this,
                    "Create transcoder with video codec \"%1\" to transcode chunk (%2 - %3) of resource %4",
                    dstCodecId, transcodeParams.startTimestamp(),
                    transcodeParams.endTimestamp(), transcodeParams.srcResourceUniqueID());
            }
    });

    return transcoder;
}

void StreamingChunkTranscoder::onTranscodingFinished(
    int /*transcodingID*/,
    bool result,
    const StreamingChunkCacheKey& key,
    DataSourceContextPtr data)
{
    using namespace std::chrono;

    // When switching hi<->lo quality hls player can delay stream for up to 20 seconds.
    static const int ADDITIONAL_TRANSCODER_LIVE_DELAY_MS = 20 * 1000;

    if (!result)
    {
        // TODO/hls: #ak Probably, incomplete chunk should be removed from cache.
        NX_VERBOSE(this, lm("Removing failed chunk transcoder. Resource %1")
            .arg(key.srcResourceUniqueID()));
        return;
    }

    m_dataSourceCache.put(
        key,
        data,
        duration_cast<milliseconds>(key.duration()).count() * 3 +
        ADDITIONAL_TRANSCODER_LIVE_DELAY_MS);  //< Ideally, <max chunk length from previous playlist> * <chunk count in playlist>
}

void StreamingChunkTranscoder::onResourceRemoved(const QnResourcePtr& resource)
{
    const auto resourceIDStr = resource->getId().toString();
    if (resourceIDStr.isEmpty())
        return;

    auto nextResourceIDStr = resourceIDStr;
    // We want to remove all cache elements having id resourceIDStr.
    // That's why we need to generate next id value.
    // We can be sure that unicode char will not overflow because nextResourceIDStr
    // contains only ascii symbols (guid).
    nextResourceIDStr[nextResourceIDStr.size() - 1] =
        QChar(nextResourceIDStr[nextResourceIDStr.size() - 1].unicode() + 1);

    m_dataSourceCache.removeRange(
        StreamingChunkCacheKey(resourceIDStr),
        StreamingChunkCacheKey(nextResourceIDStr));
}

QnVideoCameraPool* StreamingChunkTranscoder::videoCameraPool()
{
    return serverModule()->videoCameraPool();
}
