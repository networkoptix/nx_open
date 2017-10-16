#if defined(ENABLE_HANWHA)

#include "hanwha_shared_resource_context.h"
#include "hanwha_request_helper.h"
#include "hanwha_resource.h"
#include "hanwha_chunk_reader.h"

#include <media_server/media_server_module.h>
#include <core/resource_management/resource_pool.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

static const int kMaxConcurrentRequestNumber = 5;

} // namespace

using namespace nx::mediaserver::resource;

HanwhaSharedResourceContext::HanwhaSharedResourceContext(
    QnMediaServerModule* serverModule,
    const AbstractSharedResourceContext::SharedId& sharedId)
    :
    AbstractSharedResourceContext(serverModule, sharedId),
    m_sharedId(sharedId),
    m_requestSemaphore(kMaxConcurrentRequestNumber),
    m_chunkLoader(new HanwhaChunkLoader()),
    m_timeSynchronizer(new HanwhaTimeSyncronizer())
{
    m_timeSynchronizer->setTimeZoneShiftHandler(
        [this](std::chrono::seconds timeZoneShift)
        {
            m_chunkLoader->setTimeZoneShift(timeZoneShift);
        });
}

static HanwhaDeviceInfo requestAndLoadInformation(const QAuthenticator& auth, const QString& url)
{
    HanwhaDeviceInfo info{CameraDiagnostics::NoErrorResult()};
    HanwhaRequestHelper helper(auth, url);
    helper.setIgnoreMutexAnalyzer(true);

    auto deviceinfo = helper.view(lit("system/deviceinfo"));
    if (!deviceinfo.isSuccessful())
    {
        return {error(deviceinfo,
            CameraDiagnostics::CameraInvalidParams(
                lit("Can not fetch device information")))};
    }

    if (auto value = deviceinfo.parameter<QString>(lit("DeviceType")))
        info.deviceType = std::move(value->trimmed());

    if (auto value = deviceinfo.parameter<QString>(lit("FirmwareVersion")))
        info.firmware = std::move(value->trimmed());

    info.attributes = helper.fetchAttributes(lit("attributes"));
    if (!info.attributes.isValid())
    {
        return {CameraDiagnostics::CameraInvalidParams(
            lit("Camera attributes are invalid"))};
    }

    info.cgiParamiters = helper.fetchCgiParameters(lit("cgis"));
    if (!info.cgiParamiters.isValid())
    {
        return {CameraDiagnostics::CameraInvalidParams(
            lit("Camera cgi parameters are invalid"))};
    }

    auto attributes = helper.fetchAttributes(lit("attributes/System"));
    const auto maxChannels = attributes.attribute<int>(lit("System/MaxChannel"));
    info.channelCount = maxChannels ? *maxChannels : 1;

    return info;
}

HanwhaDeviceInfo HanwhaSharedResourceContext::loadInformation(const HanwhaResourcePtr& resource)
{
    const auto auth = resource->getAuth();
    const auto url = resource->getUrl();

    QnMutexLocker lock(&m_informationMutex);
    if (m_lastAuth != auth || m_lastUrl != url)
    {
        m_lastAuth = auth;
        m_lastUrl = url;
        m_cachedInformation = requestAndLoadInformation(auth, url);

        m_chunkLoader->start(auth, url, m_cachedInformation.channelCount);
        m_timeSynchronizer->start(resource);
    }

    return m_cachedInformation;
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
        auto resourcePool = serverModule()
            ->commonModule()
            ->resourcePool();

        const auto resources = resourcePool->getResourcesBySharedId(m_sharedId);
        if (resources.isEmpty())
            return QString();

        auto hanwhaResource = resources.front().dynamicCast<HanwhaResource>();
        if (!hanwhaResource)
            return QString();

        HanwhaRequestHelper helper(hanwhaResource);
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

QnSemaphore* HanwhaSharedResourceContext::requestSemaphore()
{
    return &m_requestSemaphore;
}

std::shared_ptr<HanwhaChunkLoader> HanwhaSharedResourceContext::chunkLoader() const
{
    return m_chunkLoader;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
