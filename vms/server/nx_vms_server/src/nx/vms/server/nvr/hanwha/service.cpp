#include "service.h"

#include <common/common_module.h>
#include <media_server/media_server_module.h>
#include <core/resource/media_server_resource.h>

#include <nx/vms/server/event/event_connector.h>

#include <nx/vms/server/nvr/i_manager_factory.h>

#include <nx/vms/server/nvr/hanwha/connector.h>

#include <nx/vms/server/nvr/hanwha/io/io_manager.h>
#include <nx/vms/server/nvr/hanwha/io/io_platform_abstraction.h>
#include <nx/vms/server/nvr/hanwha/io/io_module_searcher.h>

#include <nx/vms/server/nvr/hanwha/network_block/network_block_controller.h>
#include <nx/vms/server/nvr/hanwha/network_block/network_block_manager.h>
#include <nx/vms/server/nvr/hanwha/network_block/network_block_platform_abstraction.h>
#include <nx/vms/server/nvr/hanwha/network_block/dummy_powering_policy.h>

#include <nx/vms/server/nvr/hanwha/buzzer/buzzer_manager.h>
#include <nx/vms/server/nvr/hanwha/buzzer/buzzer_platform_abstraction.h>

#include <nx/vms/server/nvr/hanwha/fan/fan_manager.h>
#include <nx/vms/server/nvr/hanwha/fan/fan_platform_abstraction.h>

#include <nx/vms/server/nvr/hanwha/led/led_manager.h>
#include <nx/vms/server/nvr/hanwha/led/led_platform_abstraction.h>

namespace nx::vms::server::nvr::hanwha {

Service::Service(
    QnMediaServerModule* serverModule,
    std::unique_ptr<IManagerFactory> managerFactory,
    DeviceInfo deviceInfo)
    :
    ServerModuleAware(serverModule),
    m_managerFactory(std::move(managerFactory)),
    m_deviceInfo(std::move(deviceInfo))
{
    NX_DEBUG(this, "Creating the Hanwha NVR service");
}

Service::~Service()
{
    NX_DEBUG(this, "Destroying the Hanwha NVR service");
}

void Service::start()
{
    NX_DEBUG(this, "Starting the Hanwha NVR service");

    m_networkBlockManager = m_managerFactory->createNetworkBlockManager();
    if (!m_networkBlockManager)
    {
        NX_ERROR(this, "Unable to obtain the network block manager");
        return;
    }

    m_ioManager = m_managerFactory->createIoManager();
    if (!m_ioManager)
    {
        NX_ERROR(this, "Unable to obtain the IO manager");
        return;
    }

    m_fanManager = m_managerFactory->createFanManager();
    if (!m_fanManager)
    {
        NX_ERROR(this, "Unable to obtain the fan manager");
        return;
    }

    m_buzzerManager = m_managerFactory->createBuzzerManager();
    if (!m_buzzerManager)
    {
        NX_ERROR(this, "Unable to obtain the buzzer manager");
        return;
    }

    m_ledManager = m_managerFactory->createLedManager();
    if (!m_ledManager)
    {
        NX_ERROR(this, "Unable to obtain the LED manager");
        return;
    }

    m_connector = std::make_unique<Connector>(
        serverModule()->commonModule()->currentServer(),
        serverModule()->eventConnector(),
        m_networkBlockManager.get(),
        m_ioManager.get(),
        m_buzzerManager.get(),
        m_fanManager.get(),
        m_ledManager.get());

    m_networkBlockManager->start();
    m_ioManager->start();
    m_fanManager->start();
    m_buzzerManager->start();
}

DeviceInfo Service::deviceInfo() const
{
    return m_deviceInfo;
}

INetworkBlockManager* Service::networkBlockManager() const
{
    NX_DEBUG(this, "Network block manager has been requested");
    return m_networkBlockManager.get();
}

IIoManager* Service::ioManager() const
{
    NX_DEBUG(this, "IO manager has been requested");
    return m_ioManager.get();
}

IBuzzerManager* Service::buzzerManager() const
{
    NX_DEBUG(this, "Buzzer manager has been requested");
    return m_buzzerManager.get();
}

IFanManager* Service::fanManager() const
{
    NX_DEBUG(this, "Fan manager has been requested");
    return m_fanManager.get();
}

ILedManager* Service::ledManager() const
{
    NX_DEBUG(this, "Led manager has been requested");
    return m_ledManager.get();
}

QnAbstractResourceSearcher* Service::createSearcher() const
{
    NX_DEBUG(this, "Creating new IoModule searcher");
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
