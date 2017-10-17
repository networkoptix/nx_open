#pragma once

#if defined(ENABLE_HANWHA)

#include <QtCore/QMap>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/semaphore.h>
#include <nx/mediaserver/resource/abstract_shared_resource_context.h>
#include <nx/mediaserver/server_module_aware.h>

#include <plugins/resource/hanwha/hanwha_common.h>
#include <plugins/resource/hanwha/hanwha_time_synchronizer.h>
#include <plugins/resource/hanwha/hanwha_utils.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaChunkLoader;

struct HanwhaDeviceInfo
{
    CameraDiagnostics::Result diagnostics;
    QString deviceType;
    QString firmware;
    HanwhaAttributes attributes;
    HanwhaCgiParameters cgiParamiters;
    int channelCount = 0;

    HanwhaDeviceInfo(CameraDiagnostics::Result diagnostics = CameraDiagnostics::NotImplementedResult()):
        diagnostics(diagnostics) {}
};

class HanwhaSharedResourceContext:
    public nx::mediaserver::resource::AbstractSharedResourceContext
{

public:
    HanwhaSharedResourceContext(
        QnMediaServerModule* serverModule,
        const nx::mediaserver::resource::AbstractSharedResourceContext::SharedId& sharedId);

    HanwhaDeviceInfo loadInformation(const HanwhaResourcePtr& resource);

    QString sessionKey(
        HanwhaSessionType sessionType,
        bool generateNewOne = false);

    QnSemaphore* requestSemaphore();
    std::shared_ptr<HanwhaChunkLoader> chunkLoader() const;

private:
    mutable QnMutex m_channelCountMutex;
    
    const nx::mediaserver::resource::AbstractSharedResourceContext::SharedId m_sharedId;

    mutable QnMutex m_informationMutex;
    QAuthenticator m_lastAuth;
    QString m_lastUrl;
    HanwhaDeviceInfo m_cachedInformation;

    mutable QnMutex m_sessionMutex;
    QMap<HanwhaSessionType, QString> m_sessionKeys;

    QnSemaphore m_requestSemaphore;
    std::shared_ptr<HanwhaChunkLoader> m_chunkLoader;
    std::unique_ptr<HanwhaTimeSyncronizer> m_timeSynchronizer;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
