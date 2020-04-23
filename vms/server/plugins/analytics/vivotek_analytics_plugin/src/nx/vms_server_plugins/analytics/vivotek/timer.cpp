#include "timer.h"

#include "exception.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace std::literals;

Timer::Timer()
{
}

Timer::~Timer()
{
    pleaseStopSync();
}

cf::future<cf::unit> Timer::wait(std::chrono::milliseconds delay)
{
    return cf::make_ready_future(cf::unit()).then(
        [this, delay](auto upFuture)
        {
            upFuture.get();

            cf::promise<cf::unit> promise;
            auto future = treatBrokenPromiseAsCancelled(promise.get_future());

            m_nested.start(delay,
                [promise = std::move(promise)]() mutable
                {
                    promise.set_value(cf::unit());
                });

            return future;
        });
}

void Timer::cancel()
{
    m_nested.cancelSync();
}

void Timer::stopWhileInAioThread()
{
    BasicPollable::stopWhileInAioThread();
    m_nested.pleaseStopSync();
}

} // namespace nx::vms_server_plugins::analytics::vivotek
