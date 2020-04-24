#include "timer.h"

#include "exception.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace std::literals;
using namespace nx::network;

Timer::Timer()
{
}

Timer::~Timer()
{
}

void Timer::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    BasicPollable::bindToAioThread(aioThread);
    m_nested.bindToAioThread(aioThread);
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
