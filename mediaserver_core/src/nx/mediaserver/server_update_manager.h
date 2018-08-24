#pragma once

#include <nx/update/common_update_manager.h>
#include "server_update_installer.h"

namespace nx {
namespace mediaserver {

class ServerUpdateManager: public CommonUpdateManager
{
public:
    ServerUpdateManager(QnCommonModule* commonModule);

private:
    ServerUpdateInstaller m_installer;
    virtual vms::common::p2p::downloader::AbstractDownloader* downloader() override;
    virtual CommonUpdateInstaller* installer() override;
};

} // namespace mediaserver
} // namespace nx