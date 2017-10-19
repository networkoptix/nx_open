#pragma once

#if defined(ENABLE_HANWHA)

#include <QtCore/QMap>
#include <QtCore/QElapsedTimer>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/semaphore.h>
#include <nx/mediaserver/resource/abstract_shared_resource_context.h>
#include <nx/mediaserver/server_module_aware.h>

#include <plugins/resource/hanwha/hanwha_common.h>
#include <plugins/resource/hanwha/hanwha_time_synchronizer.h>
#include <plugins/resource/hanwha/hanwha_utils.h>
#include <nx/utils/elapsed_timer.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaChunkLoader;

struct HanwhaDeviceInfo
{
    CameraDiagnostics::Result diagnostics;
    QString deviceType;
    QString firmware;
    QString macAddress;
    QString model;
    int channelCount = 0;

    // TODO: Split into separate sructure here.

    HanwhaAttributes attributes;
    HanwhaCgiParameters cgiParamiters;

    HanwhaResponse eventStatuses;
    HanwhaResponse videoSources;
    HanwhaResponse videoProfiles;

    HanwhaDeviceInfo(
        CameraDiagnostics::Result diagnostics = CameraDiagnostics::NotImplementedResult())
        :
        diagnostics(diagnostics)
    {
    }

    bool isValid() const { return (bool) diagnostics; }
};

class HanwhaSharedResourceContext:
    public nx::mediaserver::resource::AbstractSharedResourceContext,
    public std::enable_shared_from_this<HanwhaSharedResourceContext>
{

public:
    HanwhaSharedResourceContext(
        QnMediaServerModule* serverModule,
        const nx::mediaserver::resource::AbstractSharedResourceContext::SharedId& sharedId);

    // TODO: Better to make class HanwhaAccess and keep these fields separate from context.
    void setRecourceAccess(const QUrl& url, const QAuthenticator& authenticator);
    void setLastSucessfulUrl(const QUrl& value);

    QUrl url() const;
    QAuthenticator authenticator() const;
    QnSemaphore* requestSemaphore();

    HanwhaDeviceInfo loadInformation();
    void startServices();

    QString sessionKey(
        HanwhaSessionType sessionType,
        bool generateNewOne = false);

    std::shared_ptr<HanwhaChunkLoader> chunkLoader() const;

private:
    HanwhaDeviceInfo requestAndLoadInformation();

    const nx::mediaserver::resource::AbstractSharedResourceContext::SharedId m_sharedId;

    mutable QnMutex m_dataMutex;
    QUrl m_resourceUrl;
    QAuthenticator m_resourceAuthenticator;
    QUrl m_lastSuccessfulUrl;
    nx::utils::ElapsedTimer m_lastSuccessfulUrlTimer;

    mutable QnMutex m_informationMutex;
    HanwhaDeviceInfo m_cachedInformation;
    nx::utils::ElapsedTimer m_cachedInformationTimer;

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
