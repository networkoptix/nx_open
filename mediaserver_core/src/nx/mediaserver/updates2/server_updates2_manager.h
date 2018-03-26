#pragma once

#include "nx/mediaserver/updates2/server_updates2_installer.h"
#include <common/common_module_aware.h>
#include <nx/update/manager/common_updates2_manager.h>

namespace nx {
namespace mediaserver {
namespace updates2 {

class ServerUpdates2Manager: public update::CommonUpdates2Manager
{
public:
    ServerUpdates2Manager(QnCommonModule* commonModule);
    ~ServerUpdates2Manager();

private:
    ServerUpdates2Installer m_installer;

    virtual update::info::AbstractUpdateRegistryPtr getRemoteRegistry() override;
    virtual qint64 refreshTimeout() const override;
    virtual vms::common::p2p::downloader::AbstractDownloader* downloader() override;
    virtual QString filePath() const override;
    virtual update::installer::detail::AbstractUpdates2Installer* installer() override;
};

} // namespace updates2
} // namespace mediaserver
} // namespace nx
