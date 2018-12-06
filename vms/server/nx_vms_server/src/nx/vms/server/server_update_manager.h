#pragma once

#include <nx/update/common_update_manager.h>
#include "server_update_installer.h"
#include <nx/vms/server/server_module_aware.h>

namespace nx {
namespace vms::server {

class ServerUpdateManager: public CommonUpdateManager, public ServerModuleAware
{
public:
    ServerUpdateManager(QnMediaServerModule* serverModule);

private:
    ServerUpdateInstaller m_installer;
    virtual vms::common::p2p::downloader::Downloader* downloader() override;
    virtual CommonUpdateInstaller* installer() override;
};

} // namespace vms::server
} // namespace nx