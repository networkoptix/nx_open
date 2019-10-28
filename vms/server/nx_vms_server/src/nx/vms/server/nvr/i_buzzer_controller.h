#pragma once

#include <chrono>

namespace nx::vms::server::nvr {

class IBuzzerController
{
public:
    virtual ~IBuzzerController() = default;

    /**
     * If duration is equal to zero then the buzzer should be activated until it is explicitly
     *     deactivated.
     */
    virtual bool enable(std::chrono::milliseconds duration) = 0;

    virtual bool disable() = 0;
};

} // namespace nx::vms::server::nvr
