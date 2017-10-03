#include "server_module_aware.h"

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

} // namespace mediaserver
} // namespace nx
