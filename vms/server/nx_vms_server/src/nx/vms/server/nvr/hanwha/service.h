#pragma once

#include <memory>

#include <nx/vms/server/nvr/i_service.h>

namespace nx::vms::server::nvr::hanwha {

class Service: public IService
{
public:
    Service();

    virtual IBuzzerManager* buzzerManager() override;

    virtual INetworkBlockManager* networkBlockManager() override;

    virtual IIoManager* ioManager() override;

    virtual ILedManager* ledManager() override;

private:
    std::unique_ptr<IBuzzerManager> m_buzzerManager;
    std::unique_ptr<INetworkBlockManager> m_networkBlockManager;
    std::unique_ptr<IIoManager> m_ioManager;
    std::unique_ptr<ILedManager> m_ledManager;
};

} // namespace nx::vms::server::hanwha_nvr
