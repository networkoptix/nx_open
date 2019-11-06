#pragma once

#include <memory>

#include <nx/vms/server/nvr/i_service.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server::nvr::hanwha {

class LedManager;

class Service: public IService, public nx::vms::server::ServerModuleAware
{
public:
    Service(QnMediaServerModule* serverModule);

    virtual ~Service() override;

    virtual void start() override;

    virtual IBuzzerController* buzzerController() override;

    virtual INetworkBlockController* networkBlockController() override;

    virtual IIoController* ioController() override;

    virtual ILedController* ledController() override;

    virtual QnAbstractResourceSearcher* createSearcher() override;

    virtual IService::Capabilities capabilities() const override;

private:
    std::unique_ptr<IBuzzerController> m_buzzerController;
    std::unique_ptr<INetworkBlockController> m_networkBlockController;
    std::unique_ptr<IIoController> m_ioController;
    std::unique_ptr<ILedController> m_ledController;

    std::unique_ptr<LedManager> m_ledManager;
};

} // namespace nx::vms::server::hanwha_nvr
