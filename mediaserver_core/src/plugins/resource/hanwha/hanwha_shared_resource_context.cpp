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

static const int kMaxConcurrentRequestNumber = 5;
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

    {
        QnMutexLocker lock(&m_informationMutex);
        m_cachedInformationTimer.invalidate();
    }
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

HanwhaDeviceInfo HanwhaSharedResourceContext::requestAndLoadInformation()
{
    HanwhaDeviceInfo info{CameraDiagnostics::NoErrorResult()};
    HanwhaRequestHelper helper(shared_from_this());
    helper.setIgnoreMutexAnalyzer(true);

    const auto deviceinfo = helper.view(lit("system/deviceinfo"));
    if (!deviceinfo.isSuccessful())
    {
        return error(
            deviceinfo,
            CameraDiagnostics::CameraInvalidParams(
                lit("Can not fetch device information")));
    }

    if (const auto value = deviceinfo.parameter<QString>("ConnectedMACAddress"))
        info.macAddress = *value;

    if (info.macAddress.isEmpty())
    {
        const HanwhaRequestHelper::Parameters params = {{ "interfaceName", "Network1" }};
        const auto networkInfo = helper.view("network/interface", params);
        if (!networkInfo.isSuccessful())
        {
            return error(
                deviceinfo,
                CameraDiagnostics::CameraInvalidParams(
                    lit("Can not fetch device information")));
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
        return CameraDiagnostics::CameraInvalidParams(lit("Camera attributes are invalid"));

    info.cgiParamiters = helper.fetchCgiParameters(lit("cgis"));
    if (!info.cgiParamiters.isValid())
        return CameraDiagnostics::CameraInvalidParams(lit("Camera cgi parameters are invalid"));

    if (info.deviceType == kHanwhaNvrDeviceType)
    {
        info.videoSources = helper.view(lit("media/videosource"));
        if (!info.videoSources.isSuccessful())
        {
            return error(
                info.videoSources,
                CameraDiagnostics::RequestFailedResult(
                    info.videoSources.requestUrl(),
                    info.videoSources.errorString()));
        }

        info.eventStatuses = helper.check(lit("eventstatus/eventstatus"));
        if (!info.eventStatuses.isSuccessful())
        {
            return error(
                info.eventStatuses,
                CameraDiagnostics::RequestFailedResult(
                    info.eventStatuses.requestUrl(),
                    info.eventStatuses.errorString()));
        }
    }

    info.videoProfiles = helper.view(lit("media/videoprofile"));
    const bool isCriticalError = !info.videoProfiles.isSuccessful()
        && info.videoProfiles.errorCode() != kHanwhaConfigurationNotFoundError;

    if (isCriticalError)
    {
        return CameraDiagnostics::RequestFailedResult(
            info.videoProfiles.requestUrl(),
            info.videoProfiles.errorString());
    }

    const auto maxChannels = info.attributes.attribute<int>(lit("System/MaxChannel"));
    info.channelCount = maxChannels.is_initialized() ? *maxChannels : 1;

    return info;
}

HanwhaDeviceInfo HanwhaSharedResourceContext::loadInformation()
{
    QnMutexLocker lock(&m_informationMutex);
    if (!m_cachedInformationTimer.hasExpired(kCacheDataTimeout)
        // TODO: Remove second condition and replace with different timeout in case of failure.
        && m_cachedInformation.diagnostics.errorCode == CameraDiagnostics::ErrorCode::noError)
    {
        return m_cachedInformation;
    }

    m_cachedInformation = requestAndLoadInformation();
    m_cachedInformationTimer.restart();
    return m_cachedInformation;
}

void HanwhaSharedResourceContext::startServices()
{
    {
        QnMutexLocker lock(&m_servicesMutex);
        m_chunkLoader = std::make_shared<HanwhaChunkLoader>();
        m_timeSynchronizer = std::make_unique<HanwhaTimeSyncronizer>();
        m_timeSynchronizer->setTimeZoneShiftHandler(
            [this](std::chrono::seconds timeZoneShift)
            {
                m_chunkLoader->setTimeZoneShift(timeZoneShift);
            });
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

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
