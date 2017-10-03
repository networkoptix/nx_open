#pragma once

#if defined(ENABLE_HANWHA)

#include <QtCore/QMap>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/semaphore.h>
#include <nx/mediaserver/resource/abstract_shared_resource_context.h>
#include <nx/mediaserver/server_module_aware.h>

#include <plugins/resource/hanwha/hanwha_common.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

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

private:
    mutable QnMutex m_sessionMutex;
    
    const nx::mediaserver::resource::AbstractSharedResourceContext::SharedId m_sharedId;
    QMap<HanwhaSessionType, QString> m_sessionKeys;
    QnSemaphore m_requestSemaphore;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
