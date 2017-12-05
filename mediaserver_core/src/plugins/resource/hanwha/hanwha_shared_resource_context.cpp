#if defined(ENABLE_HANWHA)

#include "hanwha_shared_resource_context.h"
#include "hanwha_request_helper.h"
#include "hanwha_resource.h"
#include "hanwha_chunk_reader.h"
#include "hanwha_common.h"

#include <media_server/media_server_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/abstract_remote_archive_manager.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

static std::array<int, (int) HanwhaSessionType::count> kMaxConsumersForType =
{
    0,  //< undefined
    10, //< live
    3,  //< archive
    1,  //< preview
    1  //< fileExport
};

namespace {

// Limited by NPM-9080VQ, it can only be opening 3 stream at the same time, while it has 4.
static const int kMaxConcurrentRequestNumber = 3;

static const std::chrono::seconds kCacheUrlTimeout(10);
static const std::chrono::seconds kCacheDataTimeout(30);

static const QUrl cleanUrl(QUrl url)
{
    url.setPath(QString());
    url.setQuery(QString());
    url.setFragment(QString());
    return url;
}

} // namespace

using namespace nx::core::resource;
using namespace nx::mediaserver::resource;

HanwhaSharedResourceContext::HanwhaSharedResourceContext(
    const AbstractSharedResourceContext::SharedId& sharedId)
    :
    information([this](){ return loadInformation(); }, kCacheDataTimeout),
    cgiParamiters([this](){ return loadCgiParamiters(); }, kCacheDataTimeout),
    eventStatuses([this](){ return loadEventStatuses(); }, kCacheDataTimeout),
    videoSources([this](){ return loadVideoSources(); }, kCacheDataTimeout),
    videoProfiles([this](){ return loadVideoProfiles(); }, kCacheDataTimeout),
    m_sharedId(sharedId),
    m_requestSemaphore(kMaxConcurrentRequestNumber)
{
}

void HanwhaSharedResourceContext::setRecourceAccess(
    const QUrl& url, const QAuthenticator& authenticator)
{
    {
        QUrl sharedUrl = cleanUrl(url);
        QnMutexLocker lock(&m_dataMutex);
        if (m_resourceUrl == sharedUrl && m_resourceAuthenticator == authenticator)
            return;

        NX_DEBUG(this, lm("Update resource access (%1:%2) %3").args(
            authenticator.user(), authenticator.password(), sharedUrl));

        m_resourceUrl = sharedUrl;
        m_resourceAuthenticator = authenticator;
        m_lastSuccessfulUrlTimer.invalidate();
    }

    information.invalidate();
    cgiParamiters.invalidate();
    eventStatuses.invalidate();
    videoSources.invalidate();
    videoProfiles.invalidate();
}

void HanwhaSharedResourceContext::setLastSucessfulUrl(const QUrl& value)
{
    QnMutexLocker lock(&m_dataMutex);
    m_lastSuccessfulUrl = cleanUrl(value);
    m_lastSuccessfulUrlTimer.restart();
}

QUrl HanwhaSharedResourceContext::url() const
{
    QnMutexLocker lock(&m_dataMutex);
    if (!m_lastSuccessfulUrlTimer.hasExpired(kCacheUrlTimeout))
        return m_lastSuccessfulUrl;
    else
        return m_resourceUrl;
}

QAuthenticator HanwhaSharedResourceContext::authenticator() const
{
    QnMutexLocker lock(&m_dataMutex);
    return m_resourceAuthenticator;
}

QnSemaphore* HanwhaSharedResourceContext::requestSemaphore()
{
    return &m_requestSemaphore;
}

void HanwhaSharedResourceContext::startServices(bool hasVideoArchive, bool isNvr)
{
    {
        QnMutexLocker lock(&m_servicesMutex);
        if (!m_timeSynchronizer)
            m_timeSynchronizer = std::make_unique<HanwhaTimeSyncronizer>();

        if (hasVideoArchive)
        {
            m_chunkLoader = std::make_shared<HanwhaChunkLoader>(this);
            m_timeSynchronizer->setTimeSynchronizationEnabled(false);
            m_timeSynchronizer->setTimeZoneShiftHandler(
                [this](std::chrono::seconds timeZoneShift)
                {
                    m_chunkLoader->setTimeZoneShift(timeZoneShift);
                    m_timeZoneShift = timeZoneShift;
                });
        }
    }

    NX_VERBOSE(this, lm("Starting services (is NVR: %1)...").arg(isNvr));
    m_timeSynchronizer->start(this);
    if (hasVideoArchive)
        m_chunkLoader->start(isNvr);
}

void HanwhaSharedResourceContext::cleanupUnsafe()
{
    for (const auto& sessionType: m_sessions.keys())
    {
        auto& contextMap = m_sessions[sessionType];
        for (auto itr = contextMap.begin(); itr != contextMap.end();)
        {
            if (itr.value().toStrongRef().isNull())
                itr = contextMap.erase(itr);
            else
                ++itr;
        }
    }
}

SessionContextPtr HanwhaSharedResourceContext::session(
    HanwhaSessionType sessionType,
    const QnUuid& clientId,
    bool generateNewOne)
{
    if (m_sharedId.isEmpty())
        return SessionContextPtr();

    QnMutexLocker lock(&m_sessionMutex);
    cleanupUnsafe();

    auto& sessionsByClientId = m_sessions[sessionType];
    const bool sessionLimitExceeded = !sessionsByClientId.contains(clientId)
        && sessionsByClientId.size() >= kMaxConsumersForType[(int)sessionType];

    if (sessionLimitExceeded)
        return SessionContextPtr();

    auto strongSessionCtx = sessionsByClientId.value(clientId).toStrongRef();
    if (strongSessionCtx)
        return strongSessionCtx;

    HanwhaRequestHelper helper(shared_from_this());
    helper.setIgnoreMutexAnalyzer(true);
    const auto response = helper.view(lit("media/sessionkey"));
    if (!response.isSuccessful())
        return SessionContextPtr();

    const auto sessionKey = response.parameter<QString>(lit("SessionKey"));
    if (sessionKey == boost::none)
        return SessionContextPtr();

    strongSessionCtx = SessionContextPtr::create(*sessionKey, clientId);
    sessionsByClientId[clientId] = strongSessionCtx.toWeakRef();

    return strongSessionCtx;
}

OverlappedTimePeriods HanwhaSharedResourceContext::overlappedTimeline(int channelNumber) const
{
    decltype(m_chunkLoader) chunkLoaderCopy;
    {
        QnMutexLocker lock(&m_servicesMutex);
        chunkLoaderCopy = m_chunkLoader;
    }

    if (chunkLoaderCopy)
        return chunkLoaderCopy->overlappedTimeline(channelNumber);

    return OverlappedTimePeriods();
}

OverlappedTimePeriods HanwhaSharedResourceContext::overlappedTimelineSync(
    int channelNumber) const
{
    decltype(m_chunkLoader) chunkLoaderCopy;
    {
        QnMutexLocker lock(&m_servicesMutex);
        chunkLoaderCopy = m_chunkLoader;
    }

    if (chunkLoaderCopy)
        return chunkLoaderCopy->overlappedTimelineSync(channelNumber);

    return OverlappedTimePeriods();
}

qint64 HanwhaSharedResourceContext::timelineStartUs(int channelNumber) const
{
    QnMutexLocker lock(&m_servicesMutex);
    if (m_chunkLoader)
        return m_chunkLoader->startTimeUsec(channelNumber);

    return AV_NOPTS_VALUE;
}

qint64 HanwhaSharedResourceContext::timelineEndUs(int channelNumber) const
{
    QnMutexLocker lock(&m_servicesMutex);
    if (m_chunkLoader)
        return m_chunkLoader->endTimeUsec(channelNumber);

    return AV_NOPTS_VALUE;
}

HanwhaResult<HanwhaInformation> HanwhaSharedResourceContext::loadInformation()
{
    HanwhaInformation info;

    HanwhaRequestHelper helper(shared_from_this());
    helper.setIgnoreMutexAnalyzer(true);

    const auto deviceinfo = helper.view(lit("system/deviceinfo"));
    if (!deviceinfo.isSuccessful())
    {
        return {error(deviceinfo,
            CameraDiagnostics::CameraInvalidParams(lit("Can not fetch device information")))};
    }

    const auto networkInfo = helper.view("network/interface", {{"interfaceName", "Network1"}});
    if (networkInfo.isSuccessful())
    {
        if (const auto value = networkInfo.parameter<QString>("MACAddress"))
            info.macAddress = *value;
    }

    if (info.macAddress.isEmpty())
    {
        if (const auto value = deviceinfo.parameter<QString>("ConnectedMACAddress"))
            info.macAddress = *value;
    }

    if (info.macAddress.isEmpty())
    {
        return {error(deviceinfo,
            CameraDiagnostics::CameraInvalidParams(lit("Can not fetch device MAC address")))};
    }

    if (const auto value = deviceinfo.parameter<QString>("Model"))
        info.model = *value;

    if (const auto value = deviceinfo.parameter<QString>(lit("DeviceType")))
        info.deviceType = value->trimmed();

    if (const auto value = deviceinfo.parameter<QString>(lit("FirmwareVersion")))
        info.firmware = value->trimmed();

    info.attributes = helper.fetchAttributes(lit("attributes"));
    if (!info.attributes.isValid())
        return {CameraDiagnostics::CameraInvalidParams(lit("Camera attributes are invalid"))};

    const auto maxChannels = info.attributes.attribute<int>(lit("System/MaxChannel"));
    info.channelCount = maxChannels.is_initialized() ? *maxChannels : 1;

    return {CameraDiagnostics::NoErrorResult(), std::move(info)};
}

HanwhaResult<HanwhaCgiParameters> HanwhaSharedResourceContext::loadCgiParamiters()
{
    HanwhaRequestHelper helper(shared_from_this());
    helper.setIgnoreMutexAnalyzer(true);

    auto cgiParamiters = helper.fetchCgiParameters(lit("cgis"));
    if (!cgiParamiters.isValid())
        return {CameraDiagnostics::CameraInvalidParams(lit("Camera cgi parameters are invalid"))};

    return {CameraDiagnostics::NoErrorResult(), std::move(cgiParamiters)};
}

HanwhaResult<HanwhaResponse> HanwhaSharedResourceContext::loadEventStatuses()
{
    HanwhaRequestHelper helper(shared_from_this());
    helper.setIgnoreMutexAnalyzer(true);

    auto eventStatuses = helper.check(lit("eventstatus/eventstatus"));
    if (!eventStatuses.isSuccessful())
    {
        return {error(
            eventStatuses,
            CameraDiagnostics::RequestFailedResult(
                eventStatuses.requestUrl(), eventStatuses.errorString()))};
    }

    return {CameraDiagnostics::NoErrorResult(), std::move(eventStatuses)};
}

HanwhaResult<HanwhaResponse> HanwhaSharedResourceContext::loadVideoSources()
{
    HanwhaRequestHelper helper(shared_from_this());
    helper.setIgnoreMutexAnalyzer(true);

    auto videoSources = helper.view(lit("media/videosource"));
    if (!videoSources.isSuccessful())
    {
        return {error(
            videoSources,
            CameraDiagnostics::RequestFailedResult(
                videoSources.requestUrl(),
                videoSources.errorString()))};
    }

    return {CameraDiagnostics::NoErrorResult(), std::move(videoSources)};
}

HanwhaResult<HanwhaResponse> HanwhaSharedResourceContext::loadVideoProfiles()
{
    HanwhaRequestHelper helper(shared_from_this());
    helper.setIgnoreMutexAnalyzer(true);

    auto videoProfiles = helper.view(lit("media/videoprofile"));
    if (!videoProfiles.isSuccessful()
        && videoProfiles.errorCode() != kHanwhaConfigurationNotFoundError)
    {
        return {CameraDiagnostics::RequestFailedResult(
            videoProfiles.requestUrl(), videoProfiles.errorString())};
    }

    return {CameraDiagnostics::NoErrorResult(), std::move(videoProfiles)};
}

std::chrono::seconds HanwhaSharedResourceContext::timeZoneShift() const
{
    return m_timeZoneShift;
}

boost::optional<int> HanwhaSharedResourceContext::overlappedId() const
{
    QnMutexLocker lock(&m_servicesMutex);
    if (!m_chunkLoader)
        return boost::none;

    return m_chunkLoader->overlappedId();
}

void HanwhaSharedResourceContext::setDateTime(const QDateTime& dateTime)
{
    m_timeSynchronizer->setDateTime(dateTime);
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
