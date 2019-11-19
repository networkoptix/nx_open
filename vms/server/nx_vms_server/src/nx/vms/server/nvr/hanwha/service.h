#pragma once

#include <memory>

#include <nx/vms/server/server_module_aware.h>

#include <nx/vms/server/nvr/i_service.h>

namespace nx::vms::server::nvr { class IManagerFactory; }

namespace nx::vms::server::nvr::hanwha {

class Connector;

class Service: public IService, ServerModuleAware
{
public:
    Service(
        QnMediaServerModule* serverModule,
        std::unique_ptr<IManagerFactory> managerFactory,
        DeviceInfo deviceInfo);

    virtual ~Service() override;

    virtual void start() override;

    virtual DeviceInfo deviceInfo() const override;

    virtual INetworkBlockManager* networkBlockManager() const override;

    virtual IIoManager* ioManager() const override;

    virtual IBuzzerManager* buzzerManager() const override;

    virtual IFanManager* fanManager() const override;

    virtual ILedManager* ledManager() const override;

    virtual QnAbstractResourceSearcher* createSearcher() const override;

    virtual IService::Capabilities capabilities() const override;

private:
    std::unique_ptr<IManagerFactory> m_managerFactory;
    DeviceInfo m_deviceInfo;

    std::unique_ptr<Connector> m_connector;

    std::unique_ptr<INetworkBlockManager> m_networkBlockManager;
    std::unique_ptr<IIoManager> m_ioManager;
    std::unique_ptr<IBuzzerManager> m_buzzerManager;
    std::unique_ptr<IFanManager> m_fanManager;
    std::unique_ptr<ILedManager> m_ledManager;
};

} // namespace nx::vms::server::hanwha_nvr
