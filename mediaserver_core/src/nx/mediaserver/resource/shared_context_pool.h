#pragma once

#include <QtCore/QMap>

#include <memory>

#include <core/resource/resource.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/std/cpp14.h>
#include <nx/mediaserver/resource/abstract_shared_resource_context.h>
#include <nx/mediaserver/server_module_aware.h>

namespace nx {
namespace mediaserver {
namespace resource {

class SharedContextPool:
    public QObject,
    public ServerModuleAware
{
    Q_OBJECT
    using SharedContextStorage = 
        QMap<
            AbstractSharedResourceContext::SharedId,
            std::shared_ptr<AbstractSharedResourceContext>>;
public:
    SharedContextPool(QnMediaServerModule* serverModule);

    template<typename ContextType>
    std::shared_ptr<ContextType> sharedContext(const QnSecurityCamResourcePtr& resource)
    {
        AbstractSharedResourceContext::SharedId sharedId = resource->getSharedId();
        if (sharedId.isEmpty())
            return nullptr;

        QnMutexLocker lock(&m_mutex);
        if (!m_sharedContexts.contains(sharedId))
            m_sharedContexts[sharedId] = std::make_shared<ContextType>(serverModule(), sharedId);

        return std::dynamic_pointer_cast<ContextType>(m_sharedContexts[sharedId]);
    };

private:
    mutable QnMutex m_mutex;
    SharedContextStorage m_sharedContexts;
};

} // namespace resource
} // namespace mediaserver
} // namespace nx
