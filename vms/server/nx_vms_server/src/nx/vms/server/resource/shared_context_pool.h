#pragma once

#include <QtCore/QMap>

#include <memory>

#include <core/resource/security_cam_resource.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/std/cpp14.h>

#include <nx/vms/server/resource/abstract_shared_resource_context.h>
#include <nx/vms/server/server_module_aware.h>

#include <media_server/media_server_module.h>

namespace nx {
namespace vms::server {
namespace resource {

class SharedContextPool:
    public QObject,
    public ServerModuleAware
{
    Q_OBJECT
    using SharedContextStorage =
        QMap<
            AbstractSharedResourceContext::SharedId,
            std::weak_ptr<AbstractSharedResourceContext>>;
public:
    SharedContextPool(QnMediaServerModule* serverModule, QObject* parent = nullptr);

    template<typename ContextType>
    std::shared_ptr<ContextType> sharedContext(const QnSecurityCamResourcePtr& resource)
    {
        AbstractSharedResourceContext::SharedId sharedId = resource->getSharedId();
        if (sharedId.isEmpty())
            return nullptr;
        return sharedContext<ContextType>(sharedId);
    }

    template<typename ContextType>
    std::shared_ptr<ContextType> sharedContext(AbstractSharedResourceContext::SharedId sharedId)
    {
        QnMutexLocker lock(&m_mutex);
        if (const auto context = m_sharedContexts.value(sharedId).lock())
            return std::dynamic_pointer_cast<ContextType>(context);

        const auto context = std::make_shared<ContextType>(
            serverModule()->globalSettings(),
            sharedId);

        m_sharedContexts[sharedId] = context;
        return context;
    }

private:
    mutable QnMutex m_mutex;
    SharedContextStorage m_sharedContexts;
};

} // namespace resource
} // namespace vms::server
} // namespace nx
