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

namespace {

// Limited by NPM-9080VQ, it can only be opening 3 stream at the same time, while it has 4.
static const int kMaxConcurrentRequestNumber = 3;

static const std::chrono::seconds kCacheUrlTimeout(10);
static const std::chrono::minutes kCacheDataTimeout(1);

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

void HanwhaSharedResourceContext::startServices()
{
    {
        QnMutexLocker lock(&m_servicesMutex);
        if (!m_chunkLoader)
        {
            NX_CRITICAL(!m_timeSynchronizer);
            m_chunkLoader = std::make_shared<HanwhaChunkLoader>();
            m_timeSynchronizer = std::make_unique<HanwhaTimeSyncronizer>();
            m_timeSynchronizer->setTimeZoneShiftHandler(
                [this](std::chrono::seconds timeZoneShift)
                {
                    m_chunkLoader->setTimeZoneShift(timeZoneShift);
                });
        }
    }

    NX_VERBOSE(this, "Starting services...");
    m_chunkLoader->start(this);
    m_timeSynchronizer->start(this);
}

QString HanwhaSharedResourceContext::sessionKey(
    HanwhaSessionType sessionType,
    bool generateNewOne)
{
    if (m_sharedId.isEmpty())
        return QString();

    QnMutexLocker lock(&m_sessionMutex);
    if (!m_sessionKeys.contains(sessionType))
    {
        HanwhaRequestHelper helper(shared_from_this());
        helper.setIgnoreMutexAnalyzer(true);
        const auto response = helper.view(lit("media/sessionkey"));
        if (!response.isSuccessful())
            return QString();

        const auto sessionKey = response.parameter<QString>(lit("SessionKey"));
        if (!sessionKey.is_initialized())
            return QString();

        m_sessionKeys[sessionType] = *sessionKey;
    }
    return m_sessionKeys.value(sessionType);
}

std::shared_ptr<HanwhaChunkLoader> HanwhaSharedResourceContext::chunkLoader() const
{
    return m_chunkLoader;
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

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
