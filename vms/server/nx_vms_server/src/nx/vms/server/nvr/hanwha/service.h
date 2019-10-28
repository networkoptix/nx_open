#pragma once

#include <memory>

#include <nx/vms/server/nvr/i_service.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server::nvr::hanwha {

class Service: public IService, public nx::vms::server::ServerModuleAware
{
public:
    Service(QnMediaServerModule* serverModule);

    virtual IBuzzerManager* buzzerManager() override;

    virtual INetworkBlockManager* networkBlockManager() override;

    virtual IIoManager* ioManager() override;

    virtual ILedManager* ledManager() override;

    virtual QnAbstractResourceSearcher* searcher() override;

    virtual IService::Capabilities capabilities() const override;

private:
    std::unique_ptr<IBuzzerManager> m_buzzerManager;
    std::unique_ptr<INetworkBlockManager> m_networkBlockManager;
    std::unique_ptr<IIoManager> m_ioManager;
    std::unique_ptr<ILedManager> m_ledManager;
    std::unique_ptr<QnAbstractResourceSearcher> m_searcher;
};

} // namespace nx::vms::server::hanwha_nvr
