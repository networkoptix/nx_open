#pragma once

#include <nx/vms/server/nvr/i_startable.h>
#include <nx/vms/server/nvr/i_buzzer_controller.h>
#include <nx/vms/server/nvr/i_network_block_controller.h>
#include <nx/vms/server/nvr/i_io_controller.h>
#include <nx/vms/server/nvr/i_led_controller.h>
#include <core/resource_management/resource_searcher.h>

namespace nx::vms::server::nvr {

class IService: public IStartable
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

    virtual IBuzzerController* buzzerController() = 0;

    virtual INetworkBlockController* networkBlockController() = 0;

    virtual IIoController* ioController() = 0;

    virtual ILedController* ledController() = 0;

    virtual QnAbstractResourceSearcher* createSearcher() = 0;

    virtual Capabilities capabilities() const = 0;
};

} // namespace nx::vms::server::nvr
