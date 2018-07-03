#pragma once

#include <common/common_module_aware.h>
#include <nx/update/manager/common_updates2_manager.h>
#include <nx/vms/common/p2p/downloader/downloader.h>

#include "client_updates2_installer.h"

namespace nx {
namespace client {
namespace desktop {
namespace updates2 {

class ClientUpdates2Manager: public nx::update::CommonUpdates2Manager
{
public:
    ClientUpdates2Manager(QnCommonModule* commonModule);
    ~ClientUpdates2Manager();

private:
    ClientUpdates2Installer m_installer;
    vms::common::p2p::downloader::Downloader m_downloader;

    virtual update::info::AbstractUpdateRegistryPtr getRemoteRegistry() override;
    virtual qint64 refreshTimeout() const override;
    virtual vms::common::p2p::downloader::AbstractDownloader* downloader() override;
    virtual QString filePath() const override;
    virtual update::installer::detail::AbstractUpdates2Installer* installer() override;
};

} // namespace updates2
} // namespace desktop
} // namespace client
} // namespace nx
