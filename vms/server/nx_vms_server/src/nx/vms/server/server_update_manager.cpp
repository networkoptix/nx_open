#include "server_update_manager.h"
#include <media_server/media_server_module.h>

namespace nx {
namespace vms::server {

ServerUpdateManager::ServerUpdateManager(QnMediaServerModule* serverModule):
    CommonUpdateManager(serverModule->commonModule()),
    ServerModuleAware(serverModule),
    m_installer(serverModule)
{
}

ServerUpdateManager::~ServerUpdateManager()
{
    m_installer.stopSync();
}

vms::common::p2p::downloader::Downloader* ServerUpdateManager::downloader()
{
    return serverModule()->findInstance<vms::common::p2p::downloader::Downloader>();
}

CommonUpdateInstaller* ServerUpdateManager::installer()
{
    return &m_installer;
}

int64_t ServerUpdateManager::freeSpace(const QString& path) const
{
    return serverModule()->rootFileSystem()->freeSpace(path);
}

} // namespace vms::server
} // namespace nx
