#include "service.h"

#include <nx/vms/server/nvr/hanwha/io_controller.h>
#include <nx/vms/server/nvr/hanwha/led_controller.h>
#include <nx/vms/server/nvr/hanwha/network_block_manager.h>
#include <nx/vms/server/nvr/hanwha/buzzer_manager.h>
#include <nx/vms/server/nvr/hanwha/io_module_searcher.h>

namespace nx::vms::server::nvr::hanwha {

Service::Service(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule),
    m_buzzerManager(std::make_unique<BuzzerManager>()),
    m_networkBlockManager(std::make_unique<NetworkBlockManager>()),
    m_ioController(std::make_unique<IoController>()),
    m_ledController(std::make_unique<LedController>()),
    m_searcher(std::make_unique<IoModuleSearcher>(serverModule))
{
}

IBuzzerManager* Service::buzzerManager()
{
    return m_buzzerManager.get();
}

INetworkBlockManager* Service::networkBlockManager()
{
    return m_networkBlockManager.get();
}

IIoController* Service::ioController()
{
    return m_ioController.get();
}

ILedController* Service::ledController()
{
    return m_ledController.get();
}

QnAbstractResourceSearcher* Service::searcher()
{
    return m_searcher.get();
}

IService::Capabilities Service::capabilities() const
{
    using Capability = IService::Capability;
    return Capabilities({
        Capability::poeManagement,
        Capability::fanMonitoring,
        Capability::leds,
        Capability::buzzer});
}

} // namespace nx::vms::server::nvr::hanwha
