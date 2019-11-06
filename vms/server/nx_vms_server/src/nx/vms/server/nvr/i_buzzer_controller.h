#pragma once

#include <chrono>

#include <nx/vms/server/nvr/i_startable.h>

namespace nx::vms::server::nvr {

class IBuzzerController: public IStartable
{
public:
    virtual ~IBuzzerController() = default;

    virtual void start() = 0;

    /**
     * If duration is equal to zero then the buzzer should be activated until it is explicitly
     *     deactivated.
     */
    virtual bool enable(std::chrono::milliseconds duration) = 0;

    virtual bool disable() = 0;
};

} // namespace nx::vms::server::nvr
