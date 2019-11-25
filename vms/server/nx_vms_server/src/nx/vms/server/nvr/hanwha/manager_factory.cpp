#include "manager_factory.h"

#include <common/common_module.h>
#include <media_server/media_server_module.h>
#include <core/resource/media_server_resource.h>

#include <nx/vms/server/root_fs.h>

#include <nx/vms/server/nvr/hanwha/hanwha_nvr_ini.h>
#include <nx/vms/server/nvr/hanwha/model_info.h>
#include <nx/vms/server/nvr/hanwha/common.h>

#include <nx/vms/server/nvr/hanwha/network_block/network_block_platform_abstraction.h>
#include <nx/vms/server/nvr/hanwha/network_block/network_block_manager.h>
#include <nx/vms/server/nvr/hanwha/network_block/dummy_powering_policy.h>

#include <nx/vms/server/nvr/hanwha/io/io_platform_abstraction.h>
#include <nx/vms/server/nvr/hanwha/io/io_manager.h>

#include <nx/vms/server/nvr/hanwha/fan/fan_platform_abstraction.h>
#include <nx/vms/server/nvr/hanwha/fan/fan_manager.h>

#include <nx/vms/server/nvr/hanwha/buzzer/buzzer_platform_abstraction.h>
#include <nx/vms/server/nvr/hanwha/buzzer/buzzer_manager.h>

#include <nx/vms/server/nvr/hanwha/led/led_platform_abstraction.h>
#include <nx/vms/server/nvr/hanwha/led/led_manager.h>

namespace nx::vms::server::nvr::hanwha {

static std::unique_ptr<IPoweringPolicy> makePoweringPolicy(const DeviceInfo& deviceInfo)
{
    const PowerConsumptionLimits powerConsumptionLimits = getPowerConsumptionLimit(deviceInfo);

    return std::make_unique<DummyPoweringPolicy>(
        powerConsumptionLimits.lowerLimitWatts,
        powerConsumptionLimits.upperLimitWatts);
}

template<typename Manager, typename PlatformAbstraction>
std::unique_ptr<Manager> makeManager(int descriptor)
{
    if (descriptor < 0)
        return nullptr;

    return std::make_unique<Manager>(std::make_unique<PlatformAbstraction>(descriptor));
}

ManagerFactory::ManagerFactory(
    QnMediaServerModule* serverModule,
    DeviceInfo deviceInfo)
    :
    ServerModuleAware(serverModule),
    m_deviceInfo(std::move(deviceInfo))
{
    NX_DEBUG(this, "Creating the manager factory");
}

ManagerFactory::~ManagerFactory()
{
    NX_DEBUG(this, "Destroying the manager factory");
    for (const auto& [_, descriptor]: m_descriptorByDeviceFileName)
        ::close(descriptor);
}

std::unique_ptr<INetworkBlockManager> ManagerFactory::createNetworkBlockManager() const
{
    NX_DEBUG(this, "Creating a network block manager");
    return std::make_unique<NetworkBlockManager>(
        serverModule()->commonModule()->currentServer(),
        std::make_unique<NetworkBlockPlatformAbstraction>(serverModule()->rootFileSystem()),
        makePoweringPolicy(m_deviceInfo));
}

std::unique_ptr<IIoManager> ManagerFactory::createIoManager() const
{
    NX_DEBUG(this, "Creating an IO manager");
    return makeManager<IoManager, IoPlatformAbstraction>(
        getDescriptorByDeviceFileName(kIoDeviceFileName));
}

std::unique_ptr<IBuzzerManager> ManagerFactory::createBuzzerManager() const
{
    NX_DEBUG(this, "Creating a buzzer manager");
    return makeManager<BuzzerManager, BuzzerPlatformAbstraction>(
        getDescriptorByDeviceFileName(kIoDeviceFileName));
}

std::unique_ptr<IFanManager> ManagerFactory::createFanManager() const
{
    NX_DEBUG(this, "Creating a fan manager");
    return makeManager<FanManager, FanPlatformAbstraction>(
        getDescriptorByDeviceFileName(kIoDeviceFileName));
}

std::unique_ptr<ILedManager> ManagerFactory::createLedManager() const
{
    NX_DEBUG(this, "Creating a LED manager");
    return makeManager<LedManager, LedPlatformAbstraction>(
        getDescriptorByDeviceFileName(kIoDeviceFileName));
}

int ManagerFactory::getDescriptorByDeviceFileName(const QString& deviceFileName) const
{
    if (const auto it = m_descriptorByDeviceFileName.find(deviceFileName);
        it != m_descriptorByDeviceFileName.cend())
    {
        NX_DEBUG(this, "Getting descriptor for '%1' from cache");
        return it->second;
    }

    if (const int descriptor =
            serverModule()->rootFileSystem()->open(deviceFileName, QIODevice::ReadWrite);
        descriptor > 0)
    {
        NX_DEBUG(this, "Device '%1' succesfully opened", deviceFileName);
        m_descriptorByDeviceFileName[deviceFileName] = descriptor;
        return descriptor;
    }

    NX_ERROR(this, "Unable to open device '%1'", deviceFileName);
    return -1;
}

} // namespace nx::vms::server::nvr::hanwha
