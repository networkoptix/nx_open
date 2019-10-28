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

    virtual IIoController* ioController() override;

    virtual ILedController* ledController() override;

    virtual QnAbstractResourceSearcher* searcher() override;

    virtual IService::Capabilities capabilities() const override;

private:
    std::unique_ptr<IBuzzerManager> m_buzzerManager;
    std::unique_ptr<INetworkBlockManager> m_networkBlockManager;
    std::unique_ptr<IIoController> m_ioController;
    std::unique_ptr<ILedController> m_ledController;
    std::unique_ptr<QnAbstractResourceSearcher> m_searcher;
};

} // namespace nx::vms::server::hanwha_nvr
