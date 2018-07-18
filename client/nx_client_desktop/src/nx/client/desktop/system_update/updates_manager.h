#pragma once

#include <common/common_module_aware.h>
#include <nx/update/manager/common_updates2_manager.h>
#include <nx/vms/common/p2p/downloader/downloader.h>

#include "updates_installer.h"

namespace nx {
namespace client {
namespace desktop {

class UpdatesManager: public nx::update::CommonUpdateManager
{
public:
    UpdatesManager(QnCommonModule* commonModule);
    ~UpdatesManager();

private:
    UpdatesInstaller m_installer;
    vms::common::p2p::downloader::Downloader m_downloader;

    virtual update::info::AbstractUpdateRegistryPtr getRemoteRegistry() override;
    virtual qint64 refreshTimeout() const override;
    virtual vms::common::p2p::downloader::AbstractDownloader* downloader() override;
    virtual QString filePath() const override;
    virtual update::installer::detail::AbstractUpdates2Installer* installer() override;
};

} // namespace desktop
} // namespace client
} // namespace nx
