#if defined(ENABLE_HANWHA)

#include "hanwha_shared_resource_context.h"
#include "hanwha_request_helper.h"
#include "hanwha_resource.h"
#include "hanwha_chunk_reader.h"
#include "hanwha_common.h"

#include <media_server/media_server_module.h>
#include <core/resource_management/resource_pool.h>
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
static const std::chrono::minutes kCacheDataTimeout(1);

static const QString kMinOverlappedDateTime = lit("2000-01-01 00:00:00");
static const QString kMaxOverlappedDateTime = lit("2038-01-01 00:00:00");

static const QUrl cleanUrl(QUrl url)
{
    url.setPath(QString());
    url.setQuery(QString());
    url.setFragment(QString());
    return url;
}

} // namespace

using namespace nx::mediaserver::resource;

HanwhaSharedResourceContext::HanwhaSharedResourceContext(
    const AbstractSharedResourceContext::SharedId& sharedId)
    :
    currentOverlappedId([this](){ return loadOverlappedId(); }, kCacheDataTimeout),
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

void HanwhaSharedResourceContext::startServices(bool hasVideoArchive)
{
    {
        QnMutexLocker lock(&m_servicesMutex);
        if (!m_timeSynchronizer)
            m_timeSynchronizer = std::make_unique<HanwhaTimeSyncronizer>();

        if (hasVideoArchive && !m_chunkLoader)
        {
            m_chunkLoader = std::make_shared<HanwhaChunkLoader>();
            m_timeSynchronizer->setTimeZoneShiftHandler(
                [this](std::chrono::seconds timeZoneShift)
                {
                    m_chunkLoader->setTimeZoneShift(timeZoneShift);
                    m_timeZoneShift = timeZoneShift;
                });
        }
    }

    NX_VERBOSE(this, lm("Starting services (has video archive: %1)...").arg(hasVideoArchive));
    m_timeSynchronizer->start(this);
    if (hasVideoArchive)
        m_chunkLoader->start(this);
}

void HanwhaSharedResourceContext::cleanupUnsafe()
{
    for (auto& context: m_sessions)
    {
        for (auto itr = context.consumers.begin(); itr != context.consumers.end();)
        {
            auto& weakGuard = itr.value();
            if (weakGuard.toStrongRef() == nullptr)
                itr = context.consumers.erase(itr);
            else
                ++itr;
        }
    }
}

SessionGuard HanwhaSharedResourceContext::session(
    HanwhaSessionType sessionType,
    const QString& clientId,
    bool generateNewOne)
{
    if (m_sharedId.isEmpty())
        return SessionGuard();

    QnMutexLocker lock(&m_sessionMutex);

    cleanupUnsafe();
    SessionContext& context = m_sessions[sessionType];
    if (context.sessionId.isEmpty())
    {
        HanwhaRequestHelper helper(shared_from_this());
        helper.setIgnoreMutexAnalyzer(true);
        const auto response = helper.view(lit("media/sessionkey"));
        if (!response.isSuccessful())
            return SessionGuard();

        const auto sessionKey = response.parameter<QString>(lit("SessionKey"));
        if (!sessionKey.is_initialized())
            return SessionGuard();

        context.sessionId = *sessionKey;
    }
    auto weakGuard = context.consumers.value(clientId);
    if (auto strongGuard = weakGuard.toStrongRef())
        return strongGuard; //< Already exists.

    // Create new one.
    if (context.consumers.size() >= kMaxConsumersForType[(int) sessionType])
        return SessionGuard();

    SessionGuard strongGuard(new char());
    context.consumers[clientId] = strongGuard.toWeakRef();
    return strongGuard;
}

QnTimePeriodList HanwhaSharedResourceContext::chunks(int channelNumber) const
{
    return m_chunkLoader->chunks(channelNumber);
}

QnTimePeriodList HanwhaSharedResourceContext::chunksSync(int channelNumber) const
{
    return m_chunkLoader->chunksSync(channelNumber);
}

qint64 HanwhaSharedResourceContext::chunksStartUsec(int channelNumber) const
{
    return m_chunkLoader->startTimeUsec(channelNumber);
}

qint64 HanwhaSharedResourceContext::chunksEndUsec(int channelNumber) const
{
    return m_chunkLoader->endTimeUsec(channelNumber);
}

HanwhaResult<int> HanwhaSharedResourceContext::loadOverlappedId()
{
    HanwhaRequestHelper helper(shared_from_this());
    helper.setIgnoreMutexAnalyzer(true);
    const auto response = helper.view(
        lit("recording/overlapped"),
        {
            {lit("FromDate"), kMinOverlappedDateTime},
            {lit("ToDate"), kMaxOverlappedDateTime}
        }); //< TODO: #dmishin it looks pretty hardcoded. Get actual time range from the camera.

    if (!response.isSuccessful())
    {
        return {error(
            response,
            CameraDiagnostics::RequestFailedResult(
                response.requestUrl(),
                response.errorString()))};
    }

    const auto overlappedIdListString = response.parameter<QString>(lit("OverlappedIDList"));
    if (!overlappedIdListString.is_initialized())
        return {CameraDiagnostics::CameraInvalidParams(lit("Can not fetch overlapped id list"))};

    const auto overlappedIds = overlappedIdListString->split(L',');
    if (overlappedIds.isEmpty())
        return {CameraDiagnostics::CameraInvalidParams(lit("Overlapped id list is empty"))};

    return {
        CameraDiagnostics::NoErrorResult(),
        overlappedIds.first().toInt() //< Check if it the first or the greatest value in the list
    };
}

HanwhaResult<HanwhaInformation> HanwhaSharedResourceContext::loadInformation()
{
    HanwhaRequestHelper helper(shared_from_this());
    helper.setIgnoreMutexAnalyzer(true);

    const auto deviceinfo = helper.view(lit("system/deviceinfo"));
    if (!deviceinfo.isSuccessful())
    {
        return {error(
            deviceinfo,
            CameraDiagnostics::CameraInvalidParams(
                lit("Can not fetch device information")))};
    }

    HanwhaInformation info;
    if (const auto value = deviceinfo.parameter<QString>("ConnectedMACAddress"))
        info.macAddress = *value;

    if (info.macAddress.isEmpty())
    {
        const HanwhaRequestHelper::Parameters params = {{ "interfaceName", "Network1" }};
        const auto networkInfo = helper.view("network/interface", params);
        if (!networkInfo.isSuccessful())
        {
            return {error(
                deviceinfo,
                CameraDiagnostics::CameraInvalidParams(
                    lit("Can not fetch device information")))};
        }

        if (const auto value = deviceinfo.parameter<QString>("MACAddress"))
            info.macAddress = *value;
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

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
