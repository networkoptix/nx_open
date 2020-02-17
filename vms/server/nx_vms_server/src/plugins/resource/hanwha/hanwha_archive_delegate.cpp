#include <media_server/media_server_module.h>
#include <nx/vms/server/resource/shared_context_pool.h>
#include <utils/common/util.h>

#include "hanwha_archive_delegate.h"
#include "hanwha_stream_reader.h"
#include "hanwha_resource.h"
#include "hanwha_shared_resource_context.h"
#include "hanwha_chunk_loader.h"
#include <nx/utils/scope_guard.h>
#include <nx/vms/server/resource/camera.h>

namespace nx {
namespace vms::server {
namespace plugins {

namespace {

// Hanwha cameras sometimes gives us first frame with timestamp that is
// greater than the requested start playback bound. So we request start time
// that is slightly earlier than one we really need.
static const std::chrono::seconds kEdgeStartTimeCorrection(1);

} // namespace

using namespace std::chrono;

HanwhaArchiveDelegate::HanwhaArchiveDelegate(const HanwhaResourcePtr& hanwhaRes):
    m_resourceContext(hanwhaRes->sharedContext())
{
    m_streamReader.reset(new HanwhaStreamReader(hanwhaRes));
    m_streamReader->setRole(Qn::CR_Archive);
    m_streamReader->setSessionType(HanwhaSessionType::archive);

    m_flags |= Flag_CanOfflineRange;
    m_flags |= Flag_CanProcessNegativeSpeed;
    m_flags |= Flag_CanOfflineLayout;
    m_flags |= Flag_CanOfflineHasVideo;
    m_flags |= Flag_UnsyncTime;
    m_flags |= Flag_CanSeekImmediatly;
}

HanwhaArchiveDelegate::~HanwhaArchiveDelegate()
{
    m_streamReader.reset();
}

bool HanwhaArchiveDelegate::open(const QnResourcePtr &/*resource*/,
    AbstractArchiveIntegrityWatcher * /*archiveIntegrityWatcher*/)
{
    m_streamReader->setRateControlEnabled(m_rateControlEnabled);
    m_lastOpenResult = m_streamReader->openStreamInternal(false, QnLiveStreamParams());
    m_sessionContext = m_streamReader->sessionContext();
    if (!m_lastOpenResult && m_errorHandler)
        m_errorHandler(lit("Can not open stream"));

    return (bool) m_lastOpenResult;
}

void HanwhaArchiveDelegate::close()
{
    m_streamReader->closeStream();
}

qint64 HanwhaArchiveDelegate::startTime() const
{
    if (m_playbackMode == PlaybackMode::Edge)
        return m_startTimeUsec + timeShiftUsec();

    // TODO: This copy-paste should probably be moved into helper function, but it is not easy
    // because with current interface we need to get both channel number and shared context.
    if (const auto resource = m_streamReader->getResource().dynamicCast<HanwhaResource>())
    {
        if (resource->getStatus() >= Qn::Online || resource->isNvr())
        {
            if (const auto context = resource->sharedContext())
                return context->timelineStartUs(resource->getChannel());
        }
    }

    return AV_NOPTS_VALUE;
}

qint64 HanwhaArchiveDelegate::endTime() const
{
    if (m_playbackMode == PlaybackMode::Edge)
        return m_endTimeUsec + timeShiftUsec();

    if (const auto resource = m_streamReader->getResource().dynamicCast<HanwhaResource>())
    {
        if (resource->getStatus() >= Qn::Online || resource->isNvr())
        {
            if (const auto context = resource->sharedContext())
                return context->timelineEndUs(resource->getChannel());
        }
    }

    return AV_NOPTS_VALUE;
}

QnAbstractMediaDataPtr HanwhaArchiveDelegate::getNextData()
{
    if (!m_streamReader)
        return QnAbstractMediaDataPtr();

    if (!m_streamReader->isStreamOpened())
    {
        const auto currentPosition = currentPositionUsec();
        if (currentPosition != AV_NOPTS_VALUE)
            m_streamReader->setPositionUsec(currentPosition);
        if (!open(m_streamReader->m_resource, /*archiveIntegrityWatcher*/ nullptr))
        {
            if (auto mediaStreamEvent = m_lastOpenResult.toMediaStreamEvent())
            {
                return QnCompressedMetadata::createMediaEventPacket(
                    isForwardDirection() ? DATETIME_NOW : 0,
                    mediaStreamEvent);
            }
            if (m_errorHandler)
                m_errorHandler(lit("Can not open stream."));
            return QnAbstractMediaDataPtr();
        }
    }

    auto result = m_streamReader->getNextData();
    if (!result)
        return result;

    bool isForwardPlayback = isForwardDirection();
    updateCurrentPositionUsec(result->timestamp, isForwardPlayback, /*force*/ false);
    if (!isForwardPlayback)
    {
        result->flags |= QnAbstractMediaData::MediaFlags_ReverseBlockStart;
        result->flags |= QnAbstractMediaData::MediaFlags_Reverse;
    }

    if (m_endTimeUsec != AV_NOPTS_VALUE && result->timestamp > m_endTimeUsec)
    {
        QnAbstractMediaDataPtr rez(new QnEmptyMediaData());
        rez->timestamp = isForwardDirection() ? DATETIME_NOW : 0;
        if (m_endOfPlaybackHandler)
            m_endOfPlaybackHandler();

        return rez;
    }

    if (result && result->dataType == QnAbstractMediaData::EMPTY_DATA)
    {
        if (auto resource = m_streamReader->getResource())
            NX_VERBOSE(this, lm("Send empty packet for camera %1").arg(resource->getUrl()));
    }

    result->timestamp += timeShiftUsec();
    return result;
}

bool HanwhaArchiveDelegate::isForwardDirection() const
{
    auto& rtspClient = m_streamReader->rtspClient();
    return rtspClient.getScale() >= 0;
}

qint64 HanwhaArchiveDelegate::timeShiftUsec() const
{
    return std::chrono::microseconds(m_resourceContext->timeShift()).count();
}

qint64 HanwhaArchiveDelegate::currentPositionUsec() const
{
    qint64 currentPosition = AV_NOPTS_VALUE;
    if (const auto sessionContext = m_sessionContext)
        currentPosition = sessionContext->currentPositionUsec();

    return currentPosition;
}

void HanwhaArchiveDelegate::updateCurrentPositionUsec(
    qint64 positionUsec, bool isForwardPlayback, bool force)
{
    if (const auto sessionContext = m_sessionContext)
        sessionContext->updateCurrentPositionUsec(positionUsec, isForwardPlayback, force);
}

qint64 HanwhaArchiveDelegate::seek(qint64 timeUsec, bool /*findIFrame*/)
{
    if (timeUsec != AV_NOPTS_VALUE)
        timeUsec -= timeShiftUsec();

    nx::utils::makeScopeGuard(
        [this, timeUsec]()
        {
            updateCurrentPositionUsec(timeUsec, isForwardDirection(), /*force*/ true);
        });

    if (!m_isSeekAlignedByChunkBorder)
    {
        m_streamReader->setPositionUsec(timeUsec);
        return timeUsec;
    }

    QnTimePeriodList chunks;
    if (const auto resource = m_streamReader->getResource().dynamicCast<HanwhaResource>())
    {
        const auto context = resource->sharedContext();
        if (resource->getStatus() >= Qn::Online && context)
        {
            const auto timeline = context->overlappedTimeline(resource->getChannel());
            NX_ASSERT(timeline.size() <= 1, lit("For NVRs there should be only one overlapped ID"));
            if (timeline.size() == 1)
                chunks = timeline.cbegin()->second;
        }
    }

    const qint64 timeMs = timeUsec / 1000;
    auto itr = chunks.findNearestPeriod(timeMs, isForwardDirection());
    if (itr == chunks.cend())
        timeUsec = isForwardDirection() ? DATETIME_NOW : 0;
    else if (!itr->contains(timeMs))
        timeUsec = isForwardDirection() ? itr->startTimeMs * 1000 : itr->endTimeMs() * 1000 - BACKWARD_SEEK_STEP;

    if (m_playbackMode == PlaybackMode::ThumbNails && currentPositionUsec() == timeUsec)
        return timeUsec; //< Ignore two thumbnails in the row from the same position.

    updateCurrentPositionUsec(timeUsec, isForwardDirection(), true);
    m_streamReader->setPositionUsec(timeUsec);
    return timeUsec;
}

QnConstResourceVideoLayoutPtr HanwhaArchiveDelegate::getVideoLayout()
{
    static QSharedPointer<QnDefaultResourceVideoLayout> videoLayout(new QnDefaultResourceVideoLayout());
    return videoLayout;
}

QnConstResourceAudioLayoutPtr HanwhaArchiveDelegate::getAudioLayout()
{
    static QSharedPointer<QnEmptyResourceAudioLayout> audioLayout(new QnEmptyResourceAudioLayout());
    return audioLayout;
}

void HanwhaArchiveDelegate::beforeClose()
{
    m_sessionContext.reset();
    m_streamReader->pleaseStop();
}

void HanwhaArchiveDelegate::setSpeed(qint64 displayTime, double value)
{
    auto& rtspClient = m_streamReader->rtspClient();
    rtspClient.setScale(value);
    if (displayTime != AV_NOPTS_VALUE)
        seek(displayTime, true /*findIFrame*/);
    else if (rtspClient.isOpened())
        rtspClient.sendPlay(AV_NOPTS_VALUE /*startTime*/, AV_NOPTS_VALUE /*endTime */, value);

    if (!m_streamReader->isStreamOpened())
        open(m_streamReader->m_resource, /*archiveIntegrityWatcher*/ nullptr);
}

void HanwhaArchiveDelegate::setRange(
    qint64 startTimeUsec, qint64 endTimeUsec, qint64 /*frameStepUsec*/)
{
    if (m_playbackMode == PlaybackMode::Edge)
        startTimeUsec -= duration_cast<microseconds>(kEdgeStartTimeCorrection).count();

    m_startTimeUsec = (startTimeUsec != AV_NOPTS_VALUE)
        ? startTimeUsec - timeShiftUsec() : AV_NOPTS_VALUE;

    m_endTimeUsec = (endTimeUsec != AV_NOPTS_VALUE)
        ? endTimeUsec - timeShiftUsec() : AV_NOPTS_VALUE;

    seek(startTimeUsec, true /*findIFrame*/);
}

void HanwhaArchiveDelegate::setOverlappedId(nx::core::resource::OverlappedId overlappedId)
{
    m_streamReader->setOverlappedId(overlappedId);
}

void HanwhaArchiveDelegate::setPlaybackMode(PlaybackMode mode)
{
    m_playbackMode = mode;
    m_isSeekAlignedByChunkBorder = false; //< I expect this variable is not required any more since we can sends empty frames before first video packet.
    auto& rtspClient = m_streamReader->rtspClient();
    rtspClient.addRequestHeader(QnRtspClient::kPlayCommand, nx::network::http::HttpHeader("Require", "onvif-replay"));
    rtspClient.addRequestHeader(QnRtspClient::kPlayCommand, nx::network::http::HttpHeader("Immediate", "yes"));
    switch (mode)
    {
        case PlaybackMode::ThumbNails:
            rtspClient.addRequestHeader(QnRtspClient::kPlayCommand, nx::network::http::HttpHeader("Frames", "Intra"));
            m_streamReader->setSessionType(HanwhaSessionType::preview);
            break;
        case PlaybackMode::Edge:
            m_isSeekAlignedByChunkBorder = false;
            [[fallthrough]];
        case PlaybackMode::Export:
            rtspClient.addRequestHeader(QnRtspClient::kPlayCommand, nx::network::http::HttpHeader("Rate-Control", "no"));
            m_streamReader->setSessionType(HanwhaSessionType::fileExport);
            break;
        default:
            break;
    }
}

void HanwhaArchiveDelegate::setGroupId(const QByteArray& id)
{
    m_streamReader->setClientId(QnUuid(id));
}

void HanwhaArchiveDelegate::beforeSeek(qint64 /*time*/)
{
    // TODO: implement
}

void HanwhaArchiveDelegate::setRateControlEnabled(bool enabled)
{
    m_rateControlEnabled = enabled;
}

bool HanwhaArchiveDelegate::setQuality(
    MediaQuality /*quality*/,
    bool /*fastSwitch*/,
    const QSize& /*resolution*/)
{
    return true;
}

CameraDiagnostics::Result HanwhaArchiveDelegate::lastError() const
{
    return m_lastOpenResult;
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
