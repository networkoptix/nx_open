#if defined(ENABLE_HANWHA)

#include <media_server/media_server_module.h>
#include <nx/mediaserver/resource/shared_context_pool.h>
#include <utils/common/util.h>

#include "hanwha_archive_delegate.h"
#include "hanwha_stream_reader.h"
#include "hanwha_resource.h"
#include "hanwha_shared_resource_context.h"
#include "hanwha_chunk_reader.h"

namespace nx {
namespace mediaserver_core {
namespace plugins {

HanwhaArchiveDelegate::HanwhaArchiveDelegate(const QnResourcePtr& resource)
{
    auto hanwhaRes = resource.dynamicCast<HanwhaResource>();
    NX_ASSERT(hanwhaRes);
    m_streamReader.reset(new HanwhaStreamReader(hanwhaRes));
    m_streamReader->setRole(Qn::CR_Archive);
    m_streamReader->setSessionType(HanwhaSessionType::archive);
    auto& rtspClient = m_streamReader->rtspClient();

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

bool HanwhaArchiveDelegate::open(const QnResourcePtr &resource)
{
    m_streamReader->setRateControlEnabled(m_rateControlEnabled);
    const auto result = (bool) m_streamReader->openStreamInternal(false, QnLiveStreamParams());
    if (!result && m_errorHandler)
        m_errorHandler(lit("Can not open stream"));

    return result;
}

void HanwhaArchiveDelegate::close()
{
    m_streamReader->closeStream();
}

qint64 HanwhaArchiveDelegate::startTime() const
{
    // TODO: This copy-paste should probably be moved into helper function, but it is not easy
    // because with current interface we need to get both channel number and shared context.
    if (const auto resource = m_streamReader->getResource().dynamicCast<HanwhaResource>())
    {
        if (resource->getStatus() >= Qn::Online)
        {
            if (const auto context = resource->sharedContext())
                return context->chunksStartUsec(resource->getChannel());
        }
    }

    return AV_NOPTS_VALUE;
}

qint64 HanwhaArchiveDelegate::endTime() const
{
    if (const auto resource = m_streamReader->getResource().dynamicCast<HanwhaResource>())
    {
        if (resource->getStatus() >= Qn::Online)
        {
            if (const auto context = resource->sharedContext())
                return context->chunksEndUsec(resource->getChannel());
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
        if (m_currentPositionUsec != AV_NOPTS_VALUE)
            m_streamReader->setPositionUsec(m_currentPositionUsec);
        if (!open(m_streamReader->m_resource))
        {
            if (m_errorHandler)
                m_errorHandler(lit("Can not open stream."));
            return QnAbstractMediaDataPtr();
        }
    }

    auto result = m_streamReader->getNextData();
    if (!result)
    {
        if (m_errorHandler)
            m_errorHandler(lit("Can not fetch data from stream"));

        return result;
    }

    m_currentPositionUsec = result->timestamp;
    if (!isForwardDirection())
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

    return result;
}

bool HanwhaArchiveDelegate::isForwardDirection() const
{
    auto& rtspClient = m_streamReader->rtspClient();
    return rtspClient.getScale() >= 0;
}

qint64 HanwhaArchiveDelegate::seek(qint64 timeUsec, bool /*findIFrame*/)
{
    QnTimePeriodList chunks;
    if (const auto resource = m_streamReader->getResource().dynamicCast<HanwhaResource>())
    {
        if (resource->getStatus() >= Qn::Online)
        {
            if (const auto context = resource->sharedContext())
                chunks = context->chunks(resource->getChannel());
        }
    }

    const qint64 timeMs = timeUsec / 1000;
    auto itr = chunks.findNearestPeriod(timeMs, isForwardDirection());
    if (itr == chunks.cend())
        timeUsec = isForwardDirection() ? DATETIME_NOW : 0;
    else if (!itr->contains(timeMs))
        timeUsec = isForwardDirection() ? itr->startTimeMs * 1000 : itr->endTimeMs() * 1000 - BACKWARD_SEEK_STEP;

    if (m_playbackMode == PlaybackMode::ThumbNails && m_currentPositionUsec == timeUsec)
        return timeUsec; //< Ignore two thumbnails in the row from the same position.

    m_currentPositionUsec = timeUsec;
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
        open(m_streamReader->m_resource);
}

void HanwhaArchiveDelegate::setRange(qint64 startTimeUsec, qint64 endTimeUsec, qint64 frameStepUsec)
{
    if (m_streamReader)
        m_streamReader->setPlaybackRange(startTimeUsec, endTimeUsec);

    m_endTimeUsec = endTimeUsec;
    seek(startTimeUsec, true /*findIFrame*/);
}

void HanwhaArchiveDelegate::setPlaybackMode(PlaybackMode mode)
{
    m_playbackMode = mode;
    auto& rtspClient = m_streamReader->rtspClient();
    switch (mode)
    {
        case PlaybackMode::ThumbNails:
            rtspClient.setAdditionAttribute("Frames", "Intra");
            m_streamReader->setSessionType(HanwhaSessionType::preview);
            break;
        case PlaybackMode::Export:
        case PlaybackMode::Edge:
            rtspClient.setAdditionAttribute("Rate-Control", "no");
            m_streamReader->setSessionType(HanwhaSessionType::fileExport);
            break;
        default:
            break;
    }
}

void HanwhaArchiveDelegate::setClientId(const QnUuid& id)
{
    m_streamReader->setClientId(id);
}

void HanwhaArchiveDelegate::beforeSeek(qint64 time)
{
    // TODO: implement me
}

void HanwhaArchiveDelegate::setRateControlEnabled(bool enabled)
{
    m_rateControlEnabled = enabled;
}

void HanwhaArchiveDelegate::setOverlappedId(int overlappedId)
{
    m_streamReader->setOverlappedId(overlappedId);
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
