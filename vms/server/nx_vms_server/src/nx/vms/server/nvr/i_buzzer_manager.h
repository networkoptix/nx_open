#pragma once

#include <chrono>

#include <nx/vms/server/nvr/types.h>

namespace nx::vms::server::nvr {

class IBuzzerManager
{
public:
    virtual ~IBuzzerManager() = default;

    virtual void start() = 0;

    virtual void stop() = 0;

    /**
     * If duration is equal to zero then the buzzer is activated until it is explicitly
     *     deactivated.
     */
    virtual bool setState(
        BuzzerState state,
        std::chrono::milliseconds duration = std::chrono::milliseconds::zero()) = 0;
};

} // namespace nx::vms::server::nvr
