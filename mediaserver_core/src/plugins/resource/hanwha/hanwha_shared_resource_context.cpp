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
    m_chunkLoader(new HanwhaChunkLoader())
{
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

int HanwhaSharedResourceContext::channelCount(const QAuthenticator& auth, const QUrl& url)
{
    QnMutexLocker lock(&m_channelCountMutex);

    if (m_cachedChannelCount)
        return m_cachedChannelCount;

    HanwhaRequestHelper helper(auth, url.toString());
    helper.setIgnoreMutexAnalyzer(true);
    auto attributes = helper.fetchAttributes(lit("attributes/System"));
    const auto maxChannels = attributes.attribute<int>(lit("System/MaxChannel"));
    int result = maxChannels ? *maxChannels : 1;
    if (attributes.isValid())
        m_cachedChannelCount = result;
    return result;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
