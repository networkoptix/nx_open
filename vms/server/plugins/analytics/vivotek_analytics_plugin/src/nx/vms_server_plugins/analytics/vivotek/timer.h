#pragma once

#include <nx/utils/thread/cf/cfuture.h>
#include <nx/network/aio/timer.h>

namespace nx::vms_server_plugins::analytics::vivotek {

class Timer:
    public nx::network::aio::Timer
{
public:
    cf::future<cf::unit> wait(std::chrono::milliseconds delay);
};

} // namespace nx::vms_server_plugins::analytics::vivotek
