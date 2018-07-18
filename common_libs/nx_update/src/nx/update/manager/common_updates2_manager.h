#pragma once

#include <common/common_module_aware.h>
#include <nx/update/manager/detail/updates2_manager_base.h>

namespace nx {
namespace update {

class NX_UPDATE_API CommonUpdates2Manager:
    public QnCommonModuleAware,
    public manager::detail::Updates2ManagerBase
{
public:
    CommonUpdates2Manager(QnCommonModule* commonModule);

protected:
    virtual QnUuid peerId() const override;

private:
    virtual void loadStatusFromFile() override;
    virtual update::info::AbstractUpdateRegistryPtr getGlobalRegistry() override;
    virtual QnUuid moduleGuid() const override;
    virtual void updateGlobalRegistry(const QByteArray& serializedRegistry) override;
    virtual void writeStatusToFile(const manager::detail::Updates2StatusDataEx& statusData) override;
    virtual void remoteUpdateCompleted() override {}
    virtual bool isClient() const override;

    void connectToSignals();
};

} // namespace update
} // namespace nx
