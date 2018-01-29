#pragma once

#include <common/common_module_aware.h>
#include <nx/mediaserver/updates2/detail/updates2_manager_base.h>

namespace nx {
namespace mediaserver {
namespace updates2 {

class Updates2Manager:
    public QnCommonModuleAware,
    public detail::Updates2ManagerBase
{
public:
    Updates2Manager(QnCommonModule* commonModule);

private:
    virtual qint64 refreshTimeout() override;
    virtual void loadStatusFromFile() override;
    virtual void connectToSignals() override;
    virtual update::info::AbstractUpdateRegistryPtr getGlobalRegistry() override;
    update::info::AbstractUpdateRegistryPtr getRemoteRegistry() override;
    virtual QnUuid moduleGuid() override;
    virtual void updateGlobalRegistry(const QByteArray& serializedRegistry) override;
    virtual void writeStatusToFile(const detail::Updates2StatusDataEx& statusData) override;
    virtual vms::common::p2p::downloader::AbstractDownloader* downloader() override;
    virtual void remoteUpdateCompleted() override {}
    virtual void downloadFinished() override {}
};

} // namespace updates2
} // namespace mediaserver
} // namespace nx
