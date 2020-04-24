#pragma once

#include <optional>

#include <nx/utils/thread/cf/cfuture.h>
#include <nx/network/aio/timer.h>

namespace nx::vms_server_plugins::analytics::vivotek {

class Timer:
    public nx::network::aio::BasicPollable
{
public:
    Timer();
    ~Timer();

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    cf::future<cf::unit> wait(std::chrono::milliseconds delay);
    void cancel();

protected:
    virtual void stopWhileInAioThread() override;

private:
    nx::network::aio::Timer m_nested;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
