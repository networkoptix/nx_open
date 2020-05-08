#include "timer.h"

#include "exception_utils.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace std::literals;
using namespace nx::network;

cf::future<cf::unit> Timer::wait(std::chrono::milliseconds delay)
{
    cf::promise<cf::unit> promise;
    auto future = promise.get_future();
    start(delay, toHandler(std::move(promise)));
    return future
        .then(translateBrokenPromiseToOperationCancelled);
}

} // namespace nx::vms_server_plugins::analytics::vivotek
