#pragma once

#include <nx/vms/server/nvr/i_buzzer_manager.h>
#include <nx/vms/server/nvr/i_network_block_controller.h>
#include <nx/vms/server/nvr/i_io_controller.h>
#include <nx/vms/server/nvr/i_led_controller.h>
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

    virtual IBuzzerManager* buzzerManager() = 0;

    virtual INetworkBlockController* networkBlockController() = 0;

    virtual IIoController* ioController() = 0;

    virtual ILedController* ledController() = 0;

    virtual QnAbstractResourceSearcher* searcher() = 0;

    virtual Capabilities capabilities() const = 0;
};

} // namespace nx::vms::server::nvr
