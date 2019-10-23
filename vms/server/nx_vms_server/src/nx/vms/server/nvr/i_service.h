#pragma once

#include <nx/vms/server/nvr/i_buzzer_manager.h>
#include <nx/vms/server/nvr/i_network_block_manager.h>
#include <nx/vms/server/nvr/i_io_manager.h>
#include <nx/vms/server/nvr/i_led_manager.h>

namespace nx::vms::server::nvr {

class IService
{
public:
    virtual ~IService() = default;

    virtual IBuzzerManager* buzzerManager() = 0;

    virtual INetworkBlockManager* networkBlockManager() = 0;

    virtual IIoManager* ioManager() = 0;

    virtual ILedManager* ledManager() = 0;
};

} // namespace nx::vms::server::nvr
