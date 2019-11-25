#pragma once

#include <nx/vms/server/nvr/types.h>
#include <nx/vms/server/nvr/i_network_block_manager.h>
#include <nx/vms/server/nvr/i_io_manager.h>
#include <nx/vms/server/nvr/i_buzzer_manager.h>
#include <nx/vms/server/nvr/i_fan_manager.h>
#include <nx/vms/server/nvr/i_led_manager.h>

#include <core/resource_management/resource_searcher.h>

namespace nx::vms::server::nvr {

class IService
{
public:
    enum class Capability
    {
        poeManagement = 1 << 0,
        fanMonitoring = 1 << 1,
        buzzer = 1 << 2,
        leds = 1 << 3,
    };

    Q_DECLARE_FLAGS(Capabilities, Capability);

public:
    virtual ~IService() = default;

    virtual void start() = 0;

    virtual void stop() = 0;

    virtual DeviceInfo deviceInfo() const = 0;

    virtual INetworkBlockManager* networkBlockManager() const = 0;

    virtual IIoManager* ioManager() const = 0;

    virtual IBuzzerManager* buzzerManager() const = 0;

    virtual IFanManager* fanManager() const = 0;

    virtual ILedManager* ledManager() const = 0;

    virtual QnAbstractResourceSearcher* createSearcher() const = 0;

    virtual Capabilities capabilities() const = 0;
};

} // namespace nx::vms::server::nvr
