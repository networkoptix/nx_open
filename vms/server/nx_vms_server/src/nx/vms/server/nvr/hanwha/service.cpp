#include "service.h"

#include <common/common_module.h>
#include <media_server/media_server_module.h>

#include <nx/vms/server/root_fs.h>

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

Service::Service(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
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

    m_networkBlockManager = std::make_unique<NetworkBlockManager>(
        serverModule()->commonModule()->currentServer(),
        std::make_unique<NetworkBlockPlatformAbstraction>(serverModule()->rootFileSystem()),
        std::make_unique<DummyPoweringPolicy>(3.0, 4.0)); //< TODO: #dmishin remove hardcode!

    int ioDeviceDescriptor =
        serverModule()->rootFileSystem()->open("/dev/ia_resource", QIODevice::ReadWrite);
    // TODO: #dmishin handle errors.

    m_ioManager = std::make_unique<IoManager>(
        std::make_unique<IoPlatformAbstraction>(ioDeviceDescriptor));
    m_fanManager = std::make_unique<FanManager>(
        std::make_unique<FanPlatformAbstraction>(ioDeviceDescriptor));
    m_buzzerManager = std::make_unique<BuzzerManager>(
        std::make_unique<BuzzerPlatformAbstraction>(ioDeviceDescriptor));
    m_ledManager = std::make_unique<LedManager>(
        std::make_unique<LedPlatformAbstraction>(ioDeviceDescriptor));

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

INetworkBlockManager* Service::networkBlockManager()
{
    NX_DEBUG(this, "Network block manager has been requested");
    return m_networkBlockManager.get();
}

IIoManager* Service::ioManager()
{
    NX_DEBUG(this, "IO manager has been requested");
    return m_ioManager.get();
}

IBuzzerManager* Service::buzzerManager()
{
    NX_DEBUG(this, "Buzzer manager has been requested");
    return m_buzzerManager.get();
}

IFanManager* Service::fanManager()
{
    NX_DEBUG(this, "Fan manager has been requested");
    return m_fanManager.get();
}

ILedManager* Service::ledManager()
{
    NX_DEBUG(this, "Led manager has been requested");
    return m_ledManager.get();
}

QnAbstractResourceSearcher* Service::createSearcher()
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
