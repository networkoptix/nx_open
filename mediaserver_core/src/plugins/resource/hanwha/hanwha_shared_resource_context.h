#pragma once

#if defined(ENABLE_HANWHA)

#include <QtCore/QMap>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/semaphore.h>
#include <nx/mediaserver/resource/abstract_shared_resource_context.h>
#include <nx/mediaserver/server_module_aware.h>

#include <plugins/resource/hanwha/hanwha_common.h>
#include <plugins/resource/hanwha/hanwha_time_synchronizer.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaChunkLoader;

class HanwhaSharedResourceContext:
    public nx::mediaserver::resource::AbstractSharedResourceContext
{

public:
    HanwhaSharedResourceContext(
        QnMediaServerModule* serverModule,
        const nx::mediaserver::resource::AbstractSharedResourceContext::SharedId& sharedId);

    QString sessionKey(
        HanwhaSessionType sessionType,
        bool generateNewOne = false);

    QnSemaphore* requestSemaphore();

    std::shared_ptr<HanwhaChunkLoader> chunkLoader() const;
    int channelCount(const QAuthenticator& auth, const QUrl& url);
    void startTimeSynchronizer(const QAuthenticator& auth, const QUrl& url);

private:
    mutable QnMutex m_sessionMutex;
    mutable QnMutex m_channelCountMutex;
    
    const nx::mediaserver::resource::AbstractSharedResourceContext::SharedId m_sharedId;
    QMap<HanwhaSessionType, QString> m_sessionKeys;
    QnSemaphore m_requestSemaphore;

    std::shared_ptr<HanwhaChunkLoader> m_chunkLoader;
    int m_cachedChannelCount = 0;

    std::unique_ptr<HanwhaTimeSyncronizer> m_timeSynchronizer;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
