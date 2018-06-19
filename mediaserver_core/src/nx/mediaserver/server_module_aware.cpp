#include "server_module_aware.h"

#include <media_server/media_server_module.h>

namespace nx {
namespace mediaserver {


ServerModuleAware::ServerModuleAware(QnMediaServerModule* serverModule):
    m_serverModule(serverModule)
{
}

ServerModuleAware::ServerModuleAware(QObject* parent)
{
    for (; parent; parent = parent->parent())
    {
        ServerModuleAware* moduleAware = dynamic_cast<ServerModuleAware*>(parent);
        if (moduleAware != nullptr)
        {
            m_serverModule = moduleAware->serverModule();
            NX_ASSERT(m_serverModule, Q_FUNC_INFO, "Invalid context");
            break;
        }

        m_serverModule = dynamic_cast<QnMediaServerModule*>(parent);
        if (m_serverModule)
            break;
    }
    NX_ASSERT(m_serverModule);
}

QnMediaServerModule* ServerModuleAware::serverModule() const
{
    return m_serverModule;
}

QnCommonModule* ServerModuleAware::commonModule() const
{
    if (!m_serverModule)
        return nullptr;

    return m_serverModule->commonModule();
}

QnResourcePool* ServerModuleAware::resourcePool() const
{
    return m_serverModule ? m_serverModule->resourcePool() : nullptr;
}

QnResourcePropertyDictionary* ServerModuleAware::propertyDictionary() const
{
    return m_serverModule ? m_serverModule->propertyDictionary() : nullptr;
}

QnCameraHistoryPool* ServerModuleAware::cameraHistoryPool() const
{
    return m_serverModule ? m_serverModule->cameraHistoryPool() : nullptr;
}

} // namespace mediaserver
} // namespace nx
