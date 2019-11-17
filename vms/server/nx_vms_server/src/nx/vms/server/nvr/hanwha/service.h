#pragma once

#include <memory>

#include <nx/vms/server/nvr/i_service.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server::nvr::hanwha {

class Connector;

class Service: public IService, public nx::vms::server::ServerModuleAware
{
public:
    Service(QnMediaServerModule* serverModule);

    virtual ~Service() override;

    virtual void start() override;

    virtual INetworkBlockManager* networkBlockManager() override;

    virtual IIoManager* ioManager() override;

    virtual IBuzzerManager* buzzerManager() override;

    virtual IFanManager* fanManager() override;

    virtual ILedManager* ledManager() override;

    virtual QnAbstractResourceSearcher* createSearcher() override;

    virtual IService::Capabilities capabilities() const override;

private:
    std::unique_ptr<Connector> m_connector;

    std::unique_ptr<INetworkBlockManager> m_networkBlockManager;
    std::unique_ptr<IIoManager> m_ioManager;
    std::unique_ptr<IBuzzerManager> m_buzzerManager;
    std::unique_ptr<IFanManager> m_fanManager;
    std::unique_ptr<ILedManager> m_ledManager;
};

} // namespace nx::vms::server::hanwha_nvr
