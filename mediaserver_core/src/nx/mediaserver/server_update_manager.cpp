#include "server_update_manager.h"
#include <media_server/media_server_module.h>

namespace nx {
namespace mediaserver {

ServerUpdateManager::ServerUpdateManager(QnCommonModule* commonModule):
    CommonUpdateManager(commonModule)
{
}

vms::common::p2p::downloader::AbstractDownloader* ServerUpdateManager::downloader()
{
    return qnServerModule->findInstance<vms::common::p2p::downloader::Downloader>();
}

CommonUpdateInstaller* ServerUpdateManager::installer()
{
    return &m_installer;
}

} // namespace mediaserver
} // namespace nx