#if defined(ENABLE_HANWHA)

#include <media_server/media_server_module.h>
#include <nx/mediaserver/resource/shared_context_pool.h>

#include "hanwha_archive_delegate.h"
#include "hanwha_stream_reader.h"
#include "hanwha_resource.h"
#include "hanwha_shared_resource_context.h"
#include "hanwha_chunk_reader.h"

namespace nx {
namespace mediaserver_core {
namespace plugins {

HanwhaNvrArchiveDelegate::HanwhaNvrArchiveDelegate(const QnResourcePtr& resource)
{
    auto hanwhaRes = resource.dynamicCast<HanwhaResource>();
    NX_ASSERT(hanwhaRes);
    m_streamReader.reset(new HanwhaStreamReader(hanwhaRes));
    m_streamReader->setRole(Qn::CR_Archive);

    m_flags |= Flag_CanOfflineRange;
    //m_flags |= Flag_CanProcessNegativeSpeed; //< TODO: implement me
    m_flags |= Flag_CanProcessMediaStep;
    m_flags |= Flag_CanOfflineLayout;
    m_flags |= Flag_CanOfflineHasVideo;
    m_flags |= Flag_UnsyncTime;
    m_flags |= Flag_CanSeekImmediatly;
}

HanwhaNvrArchiveDelegate::~HanwhaNvrArchiveDelegate()
{
    m_streamReader.reset();
}

bool HanwhaNvrArchiveDelegate::open(const QnResourcePtr &resource)
{
    return (bool) m_streamReader->openStreamInternal(false, QnLiveStreamParams());
}

void HanwhaNvrArchiveDelegate::close()
{
    m_streamReader->closeStream();
}

qint64 HanwhaNvrArchiveDelegate::startTime() const
{
    auto hanwhaRes = m_streamReader->getResource().dynamicCast<HanwhaResource>();
    return qnServerModule->sharedContextPool()
        ->sharedContext<HanwhaSharedResourceContext>(hanwhaRes)
        ->chunkLoader()->startTimeUsec(hanwhaRes->getChannel());
}

qint64 HanwhaNvrArchiveDelegate::endTime() const
{
    auto hanwhaRes = m_streamReader->getResource().dynamicCast<HanwhaResource>();
    return qnServerModule->sharedContextPool()
        ->sharedContext<HanwhaSharedResourceContext>(hanwhaRes)
        ->chunkLoader()->endTimeUsec(hanwhaRes->getChannel());
}

QnAbstractMediaDataPtr HanwhaNvrArchiveDelegate::getNextData()
{
    if (!m_streamReader)
        return QnAbstractMediaDataPtr();
    return m_streamReader->getNextData();
}

qint64 HanwhaNvrArchiveDelegate::seek(qint64 timeUsec, bool /*findIFrame*/)
{
    auto hanwhaRes = m_streamReader->getResource().dynamicCast<HanwhaResource>();
    const auto chunks = qnServerModule->sharedContextPool()
        ->sharedContext<HanwhaSharedResourceContext>(hanwhaRes)
        ->chunkLoader()->chunks(hanwhaRes->getChannel());
    const bool isForwardDirection = m_scale >= 0;
    auto itr = chunks.findNearestPeriod(timeUsec / 1000, isForwardDirection);
    if (itr == chunks.cend())
        timeUsec = isForwardDirection ? DATETIME_NOW : 0;
    else
        timeUsec = itr->startTimeMs * 1000;


    m_streamReader->setPositionUsec(timeUsec);
    return timeUsec;
}

QnConstResourceVideoLayoutPtr HanwhaNvrArchiveDelegate::getVideoLayout()
{
    static QSharedPointer<QnDefaultResourceVideoLayout> videoLayout(new QnDefaultResourceVideoLayout());
    return videoLayout;
}

QnConstResourceAudioLayoutPtr HanwhaNvrArchiveDelegate::getAudioLayout()
{
    static QSharedPointer<QnEmptyResourceAudioLayout> audioLayout(new QnEmptyResourceAudioLayout());
    return audioLayout;
}

void HanwhaNvrArchiveDelegate::beforeClose()
{
    m_streamReader->pleaseStop();
}

void HanwhaNvrArchiveDelegate::onReverseMode(qint64 displayTime, bool value)
{
    // TODO: implement me
}

void HanwhaNvrArchiveDelegate::setRange(qint64 startTime, qint64 endTime, qint64 frameStep)
{
    // TODO: implement me
}

void HanwhaNvrArchiveDelegate::beforeSeek(qint64 time)
{
    // TODO: implement me
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
