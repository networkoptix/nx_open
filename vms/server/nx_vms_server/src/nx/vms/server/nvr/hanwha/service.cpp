#include "service.h"

#include <nx/vms/server/nvr/hanwha/io_controller.h>
#include <nx/vms/server/nvr/hanwha/led_controller.h>
#include <nx/vms/server/nvr/hanwha/network_block_controller.h>
#include <nx/vms/server/nvr/hanwha/buzzer_controller.h>
#include <nx/vms/server/nvr/hanwha/io_module_searcher.h>

namespace nx::vms::server::nvr::hanwha {

Service::Service(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule),
    m_buzzerController(std::make_unique<BuzzerController>()),
    m_networkBlockController(std::make_unique<NetworkBlockController>()),
    m_ioController(std::make_unique<IoController>()),
    m_ledController(std::make_unique<LedController>()),
    m_searcher(std::make_unique<IoModuleSearcher>(serverModule))
{
}

IBuzzerController* Service::buzzerController()
{
    return m_buzzerController.get();
}

INetworkBlockController* Service::networkBlockController()
{
    return m_networkBlockController.get();
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
