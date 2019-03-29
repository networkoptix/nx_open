#include "gop_keeper.h"

#include <api/global_settings.h>
#include <nx/utils/random.h>
#include <utils/memory/cyclic_allocator.h>
#include <utils/common/synctime.h>
#include <common/common_module.h>
#include <camera/video_camera.h>

namespace {

constexpr qint64 kCameraUpdateInterval = 1000000ll * 3600;
constexpr qint64 kKeepIframesInterval = 1000000ll * 80;
constexpr qint64 kKeepIframesDistance = 1000000ll * 5;
constexpr qint64 kGetFrameMaxTime = 1000000ll * 15;
constexpr int kMaxGopSize = 260;

} // namespace

namespace nx {
namespace vms {
namespace server {

GopKeeper::GopKeeper(
    QnVideoCamera* camera,
    const QnResourcePtr& resource,
    QnServer::ChunksCatalog catalog)
    :
    QnResourceConsumer(resource),
    QnAbstractDataConsumer(kMaxGopSize),
    m_lastKeyFrameChannel(0),
    m_gotIFramesMask(0),
    m_allChannelsMask(0),
    m_camera(camera),
    m_activityStarted(false),
    m_activityStartTime(0),
    m_catalog(catalog),
    m_nextMinTryTime(0)
{
}

GopKeeper::~GopKeeper()
{
    stop();
}

void GopKeeper::beforeDisconnectFromResource()
{
    pleaseStop();
}

bool GopKeeper::canAcceptData() const
{
    return true;
}

static CyclicAllocator gopKeeperKeyFramesAllocator;

static QnAbstractAllocator* getAllocator(size_t frameSize)
{
    const static size_t kMaxFrameSize = CyclicAllocator::DEFAULT_ARENA_SIZE / 2;
    return frameSize < kMaxFrameSize
        ? static_cast<QnAbstractAllocator*>(&gopKeeperKeyFramesAllocator)
        : static_cast<QnAbstractAllocator*>(QnSystemAllocator::instance());
}

void GopKeeper::putData(const QnAbstractDataPacketPtr& nonConstData)
{
    if (m_allChannelsMask == 0)
    {
        auto layout = qSharedPointerDynamicCast<QnMediaResource>(m_resource)->getVideoLayout();
        m_allChannelsMask = (1 << layout->channelCount()) - 1;
    }

    QnMutexLocker lock(&m_queueMtx);
    if (QnConstCompressedVideoDataPtr video =
        std::dynamic_pointer_cast<const QnCompressedVideoData>(nonConstData))
    {
        NX_VERBOSE(this, "%1(%2 us, %3-frame)",
            __func__, nonConstData->timestamp, (video->flags & AV_PKT_FLAG_KEY) ? "I" : "P");

        if (video->flags & AV_PKT_FLAG_KEY)
        {
            if (m_gotIFramesMask == m_allChannelsMask)
            {
                m_dataQueue.clear();
                m_gotIFramesMask = 0;
            }
            m_gotIFramesMask |= 1 << video->channelNumber;
            int ch = video->channelNumber;
            m_lastKeyFrame[ch] = video;
            const qint64 removeThreshold = video->timestamp - kKeepIframesInterval;

            if (m_lastKeyFrames[ch].empty()
                || m_lastKeyFrames[ch].back()->timestamp <=
                    video->timestamp - kKeepIframesDistance)
            {
                m_lastKeyFrames[ch].push_back(QnCompressedVideoDataPtr(
                    video->clone(getAllocator(video->dataSize()))));
            }

            while ((!m_lastKeyFrames[ch].empty()
                && m_lastKeyFrames[ch].front()->timestamp < removeThreshold)
                || m_lastKeyFrames[ch].size() > kKeepIframesInterval/kKeepIframesDistance)
            {
                m_lastKeyFrames[ch].pop_front();
            }
        }

        if (m_dataQueue.size() < m_dataQueue.maxSize())
        {
            // TODO: #ak: MUST NOT modify video packet here! It can be used by other threads
            // concurrently and flags value can be undefined in other threads.
            static_cast<QnAbstractMediaData*>(nonConstData.get())->flags |=
                QnAbstractMediaData::MediaFlags_LIVE;
            QnAbstractDataConsumer::putData(nonConstData);
        }
    }
    else if (QnConstCompressedAudioDataPtr audio =
        std::dynamic_pointer_cast<const QnCompressedAudioData>(nonConstData))
    {
        NX_VERBOSE(this, "%1(%2 us, audio)", __func__, nonConstData->timestamp);
        m_lastAudioData = std::move(audio);
    }
    else
    {
        NX_VERBOSE(this, "%1(%2 us): Ignored unsupported data", __func__, nonConstData->timestamp);
    }
}

bool GopKeeper::processData(const QnAbstractDataPacketPtr& /*data*/)
{
    return true;
}

int GopKeeper::copyLastGop(qint64 skipTime, QnDataPacketQueue& dstQueue, bool iFramesOnly)
{
    auto addData =
        [&](const QnConstAbstractDataPacketPtr& data)
        {
            const QnCompressedVideoData* video =
                dynamic_cast<const QnCompressedVideoData*>(data.get());
            if (video)
            {
                QnCompressedVideoData* newData = video->clone();
                if (skipTime && video->timestamp <= skipTime)
                    newData->flags |= QnAbstractMediaData::MediaFlags_Ignore;

                // data->opaque is used here to designate cseq (Command Sequence Number, see
                // QnDataConsumer::setWaitCSeq for details).
                // When playing live stream, several frames from the GopKeeper are pushed in the
                // stream  before real live frames are played. Since live frames always have
                // cseq == 0, we force GopKeeper frames to have the same cseq.
                newData->opaque = 0;
                dstQueue.push(QnAbstractMediaDataPtr(newData));
            }
            else
            {
                // TODO: #akolesnikov: Remove const_cast.
                dstQueue.push(std::const_pointer_cast<QnAbstractDataPacket>(data));
            }
        };

    int rez = 0;
    if (iFramesOnly)
    {
        QnMutexLocker lock(&m_queueMtx);
        for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        {
            if (m_lastKeyFrame[i])
            {
                addData(m_lastKeyFrame[i]);
                ++rez;
            }
        }
    }
    else
    {
        auto randomAccess = m_dataQueue.lock();
        for (int i = 0; i < randomAccess.size(); ++i)
        {
            addData(randomAccess.at(i));
            ++rez;
        }
    }
    return rez;
}

QnConstCompressedVideoDataPtr GopKeeper::getIframeByTimeUnsafe(
    qint64 timeUs, int channel, nx::api::ImageRequest::RoundMethod roundMethod) const
{
    const auto &queue = m_lastKeyFrames[channel];
    if (queue.empty())
        return QnConstCompressedVideoDataPtr(); // no video data

    auto condition =
        [](const QnConstCompressedVideoDataPtr& data, qint64 time)
        {
            return data->timestamp < time;
        };

    auto itr = std::lower_bound(
        queue.begin(),
        queue.end(),
        timeUs,
        condition);

    if (itr == queue.end())
    {
        const bool returnLastKeyFrame =
            m_lastKeyFrame[channel]
            && (m_lastKeyFrame[channel]->timestamp <= timeUs
                || roundMethod != nx::api::ImageRequest::RoundMethod::iFrameBefore);

        if (returnLastKeyFrame)
            return m_lastKeyFrame[channel];
        else
            return queue.back();
    }

    const bool returnIframeBeforeTime =
        itr != queue.begin()
        && (*itr)->timestamp > timeUs
        && roundMethod == nx::api::ImageRequest::RoundMethod::iFrameBefore;

    if (returnIframeBeforeTime)
        --itr; // prefer frame before defined time if no exact match

    return QnConstCompressedVideoDataPtr((*itr)->clone());
}

QnConstCompressedVideoDataPtr GopKeeper::getIframeByTime(
    qint64 timeUs, int channel, nx::api::ImageRequest::RoundMethod roundMethod) const
{
    QnMutexLocker lock( &m_queueMtx );
    return getIframeByTimeUnsafe(timeUs, channel, roundMethod);
}

/** @return Frames from I-frame up to the desired one. Can be empty but not null. */
std::unique_ptr<QnConstDataPacketQueue> GopKeeper::getGopTillTime(
    qint64 timeUs, int channel) const
{
    NX_VERBOSE(this, "%1(%2 us, channel: %3) BEGIN", __func__, timeUs, channel);

    QnMutexLocker lock(&m_queueMtx);

    auto frames = std::make_unique<QnConstDataPacketQueue>();
    const auto dataQueue = m_dataQueue.lock();
    for (int i = 0; i < dataQueue.size(); ++i)
    {
        const QnConstAbstractDataPacketPtr& data = dataQueue.at(i);
        auto video = std::dynamic_pointer_cast<const QnCompressedVideoData>(data);
        const qint64 tUs = video->timestamp;
        if (video && tUs <= timeUs && video->channelNumber == (quint32) channel)
        {
            NX_VERBOSE(this, "%1(): Adding frame %2 (%3%4) us", __func__,
                tUs, (tUs >= timeUs) ? "+" : "-", std::abs(tUs - timeUs));
            frames->push(video);
        }
        else
        {
            NX_VERBOSE(this, "%1(): Skipping frame %2 (%3%4) us", __func__,
                tUs, (tUs >= timeUs) ? "+" : "-", std::abs(tUs - timeUs));
        }
    }

    if (frames->isEmpty())
        NX_VERBOSE(this, "%1() END -> empty", __func__);
    else
        NX_VERBOSE(this, "%1() END -> %2 frame(s)", __func__, frames->size());
    return frames;
}

std::unique_ptr<QnConstDataPacketQueue> GopKeeper::getFrameSequenceByTime(
    qint64 timeUs, int channel, nx::api::ImageRequest::RoundMethod roundMethod) const
{
    NX_VERBOSE(this, "%1(%2 us, channel: %3, %4) BEGIN", __func__, timeUs, channel, roundMethod);
    if (roundMethod == nx::api::ImageRequest::RoundMethod::precise)
    {
        auto frames = getGopTillTime(timeUs, channel);
        if (frames->isEmpty())
        {
            NX_VERBOSE(this, "%1() END -> null: No 'precise' in GopKeeper", __func__);
            return nullptr;
        }

        NX_VERBOSE(this, "%1() END -> %2 frame(s): Got 'precise' from GopKeeper", __func__,
            frames->size());
        return frames;
    }

    auto iFrame = getIframeByTime(timeUs, channel, roundMethod);
    if (!iFrame)
    {
        NX_VERBOSE(this, "%1() END -> null: No I-frame in GopKeeper", __func__);
        return nullptr;
    }

    if (roundMethod == nx::api::ImageRequest::RoundMethod::iFrameAfter && iFrame->timestamp < timeUs)
    {
        NX_VERBOSE(this, "%1(): Got 'before' I-frame but requested 'after' => attempt 'precise'",
            __func__);
        auto frames = getGopTillTime(timeUs, channel);
        if (frames->isEmpty())
        {
            frames->push(iFrame);
            NX_VERBOSE(this, "%1() END -> iFrame 'before': No 'precise' in GopKeeper", __func__);
            return frames;
        }

        NX_VERBOSE(this, "%1() END -> %2 frame(s): Got 'after' from GopKeeper", __func__,
            frames->size());
        return frames;
    }

    if (roundMethod == nx::api::ImageRequest::RoundMethod::iFrameBefore && iFrame->timestamp > timeUs)
    {
        NX_VERBOSE(this, "%1() END -> null: Got 'after' I-frame but requested 'before'", __func__);
        return nullptr;
    }

    auto frames = std::make_unique<QnConstDataPacketQueue>();
    frames->push(iFrame);
    NX_VERBOSE(this, "%1() END -> iFrame", __func__);
    return frames;
}

QnConstCompressedVideoDataPtr GopKeeper::getLastVideoFrame(int channel) const
{
    QnMutexLocker lock( &m_queueMtx );
    return m_lastKeyFrame[channel];
}

QnConstCompressedAudioDataPtr GopKeeper::getLastAudioFrame() const
{
    QnMutexLocker lock( &m_queueMtx );
    return m_lastAudioData;
}

void GopKeeper::updateCameraActivity()
{
    qint64 usecTime = getUsecTimer();
    qint64 lastKeyTime = AV_NOPTS_VALUE;
    {
        QnMutexLocker lock( &m_queueMtx );
        if (m_lastKeyFrame[0])
            lastKeyTime = m_lastKeyFrame[0]->timestamp;
    }

    const QnSecurityCamResource* cameraResource =
        dynamic_cast<QnSecurityCamResource*>(m_resource.data());
    bool canUseProvider = m_catalog == QnServer::HiQualityCatalog
        || !cameraResource || cameraResource->hasDualStreaming();

    if (!m_resource->hasFlags(Qn::foreigner) && m_resource->isInitialized() && canUseProvider &&
       (lastKeyTime == (qint64)AV_NOPTS_VALUE
            || qnSyncTime->currentUSecsSinceEpoch() - lastKeyTime > kCameraUpdateInterval) &&
        m_resource->commonModule()->globalSettings()->isAutoUpdateThumbnailsEnabled())
    {
        if (m_nextMinTryTime == 0) // get first screenshot after minor delay
            m_nextMinTryTime = usecTime + nx::utils::random::number(5000, 10000) * 1000ll;

        if (!m_activityStarted && usecTime > m_nextMinTryTime) {
            m_activityStarted = true;
            m_activityStartTime = usecTime;
            m_nextMinTryTime = usecTime + nx::utils::random::number(100, 100 + 60*5) * 1000000ll;
            m_camera->inUse(this);
            QnLiveStreamProviderPtr provider = m_camera->getLiveReader(m_catalog);
            if (provider)
                provider->startIfNotRunning();
        }
        else if (m_activityStarted && usecTime - m_activityStartTime > kGetFrameMaxTime )
        {
            m_activityStarted = false;
            m_camera->notInUse(this);
        }
    }
    else {
        if (m_activityStarted) {
            m_activityStarted = false;
            m_camera->notInUse(this);
        }
    }

}

void GopKeeper::clearVideoData()
{
    QnMutexLocker lock( &m_queueMtx );

    for( QnConstCompressedVideoDataPtr& frame: m_lastKeyFrame )
        frame.reset();
    for( auto& lastKeyFramesForChannel: m_lastKeyFrames )
        lastKeyFramesForChannel.clear();
    m_nextMinTryTime = 0;
}

} // namespace server
} // namespace vms
} // namespace nx
