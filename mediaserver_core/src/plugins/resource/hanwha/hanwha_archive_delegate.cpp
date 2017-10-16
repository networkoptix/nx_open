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
    rtspClient.setAdditionAttribute("Rate-Control", "no");

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
    return (bool) m_streamReader->openStreamInternal(false, QnLiveStreamParams());
}

void HanwhaArchiveDelegate::close()
{
    m_streamReader->closeStream();
}

qint64 HanwhaArchiveDelegate::startTime() const
{
    auto hanwhaRes = m_streamReader->getResource().dynamicCast<HanwhaResource>();
    return qnServerModule->sharedContextPool()
        ->sharedContext<HanwhaSharedResourceContext>(hanwhaRes)
        ->chunkLoader()->startTimeUsec(hanwhaRes->getChannel());
}

qint64 HanwhaArchiveDelegate::endTime() const
{
    auto hanwhaRes = m_streamReader->getResource().dynamicCast<HanwhaResource>();
    return qnServerModule->sharedContextPool()
        ->sharedContext<HanwhaSharedResourceContext>(hanwhaRes)
        ->chunkLoader()->endTimeUsec(hanwhaRes->getChannel());
}

QnAbstractMediaDataPtr HanwhaArchiveDelegate::getNextData()
{
    if (!m_streamReader)
        return QnAbstractMediaDataPtr();
    if (!m_streamReader->isStreamOpened() && !open(m_streamReader->m_resource))
        return QnAbstractMediaDataPtr();

    auto result = m_streamReader->getNextData();
    if (result && !isForwardDirection())
    {
        result->flags |= QnAbstractMediaData::MediaFlags_ReverseBlockStart;
        result->flags |= QnAbstractMediaData::MediaFlags_Reverse;
    }

    if (result && m_endTimeUsec != AV_NOPTS_VALUE && result->timestamp > m_endTimeUsec)
    {
        QnAbstractMediaDataPtr rez(new QnEmptyMediaData());
        rez->timestamp = isForwardDirection() ? DATETIME_NOW : 0;
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
    auto hanwhaRes = m_streamReader->getResource().dynamicCast<HanwhaResource>();
    const auto chunks = qnServerModule->sharedContextPool()
        ->sharedContext<HanwhaSharedResourceContext>(hanwhaRes)
        ->chunkLoader()->chunks(hanwhaRes->getChannel());
    const qint64 timeMs = timeUsec / 1000;
    auto itr = chunks.findNearestPeriod(timeMs, isForwardDirection());
    if (itr == chunks.cend())
        timeUsec = isForwardDirection() ? DATETIME_NOW : 0;
    else if (!itr->contains(timeMs))
        timeUsec = isForwardDirection() ? itr->startTimeMs * 1000 : itr->endTimeMs() * 1000 - BACKWARD_SEEK_STEP;

    if (m_previewMode && m_lastSeekTime == timeUsec)
        return timeUsec;

    m_lastSeekTime = timeUsec;
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

void HanwhaArchiveDelegate::onReverseMode(qint64 displayTime, bool value)
{
    auto& rtspClient = m_streamReader->rtspClient();
    rtspClient.setScale(value ? -1 : 1);
    seek(displayTime, true /*findIFrame*/);
    close();
    open(m_streamReader->m_resource);
}

void HanwhaArchiveDelegate::setRange(qint64 startTimeUsec, qint64 endTimeUsec, qint64 frameStepUsec)
{
    m_endTimeUsec = endTimeUsec;
    m_previewMode = frameStepUsec > 1;
    if (m_previewMode)
    {
        auto& rtspClient = m_streamReader->rtspClient();
        rtspClient.setAdditionAttribute("Frames", "Intra");
    }
    m_streamReader->setSessionType(
        m_previewMode ? HanwhaSessionType::preview : HanwhaSessionType::archive);
    seek(startTimeUsec, true /*findIFrame*/);
}

void HanwhaArchiveDelegate::beforeSeek(qint64 time)
{
    // TODO: implement me
}

void HanwhaArchiveDelegate::setRateControlEnabled(bool enabled)
{
    m_rateControlEnabled = enabled;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
