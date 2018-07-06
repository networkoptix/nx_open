#include "server_module_aware.h"

#include <media_server/media_server_module.h>

namespace nx {
namespace mediaserver {


ServerModuleAware::ServerModuleAware(QnMediaServerModule* serverModule):
    m_serverModule(serverModule)
{
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

} // namespace mediaserver
} // namespace nx
