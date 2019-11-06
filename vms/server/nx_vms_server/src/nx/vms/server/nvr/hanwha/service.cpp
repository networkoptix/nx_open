#include "service.h"

#include <nx/vms/server/nvr/hanwha/io_controller.h>
#include <nx/vms/server/nvr/hanwha/led_controller.h>
#include <nx/vms/server/nvr/hanwha/network_block_controller.h>
#include <nx/vms/server/nvr/hanwha/buzzer_controller.h>
#include <nx/vms/server/nvr/hanwha/io_module_searcher.h>
#include <nx/vms/server/nvr/hanwha/led_manager.h>

namespace nx::vms::server::nvr::hanwha {

Service::Service(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule),
    m_buzzerController(std::make_unique<BuzzerController>()),
    m_networkBlockController(std::make_unique<NetworkBlockController>()),
    m_ioController(std::make_unique<IoController>()),
    m_ledController(std::make_unique<LedController>()),
    m_ledManager(std::make_unique<LedManager>(serverModule))
{
}

Service::~Service()
{
    // TODO: #dmishin implement
}

void Service::start()
{
    m_buzzerController->start();
    m_networkBlockController->start();
    m_ioController->start();
    m_ledController->start();
    m_ledManager->start();
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

QnAbstractResourceSearcher* Service::createSearcher()
{
    return new IoModuleSearcher(serverModule());
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
