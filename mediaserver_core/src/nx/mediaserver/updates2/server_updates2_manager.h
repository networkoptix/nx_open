#pragma once

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
    virtual update::info::AbstractUpdateRegistryPtr getRemoteRegistry() override;
    virtual void connectToSignals() override;
    virtual qint64 refreshTimeout() const override;
    virtual vms::common::p2p::downloader::AbstractDownloader* downloader() override;
    virtual QString filePath() const override;
    virtual update::detail::AbstractUpdates2InstallerPtr installer() override;
};

} // namespace updates2
} // namespace mediaserver
} // namespace nx
