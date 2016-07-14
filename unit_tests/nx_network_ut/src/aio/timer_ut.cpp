/**********************************************************
* Mar 17, 2016
* a.kolesnikov
***********************************************************/

#include <nx/network/aio/timer.h>

#include <gtest/gtest.h>

#include <nx/utils/std/future.h>


namespace nx {
namespace network {
namespace aio {

TEST(aioTimer, modify_timeout)
{
    for (int i = 0; i < 29; ++i)
    {
        aio::Timer timer;
        nx::utils::promise<void> timedoutPromise;
        auto timeoutHandler = [&timedoutPromise]{ timedoutPromise.set_value(); };
        timer.start(std::chrono::seconds(250), timeoutHandler);
        timer.start(std::chrono::milliseconds(250), timeoutHandler);

        ASSERT_EQ(
            std::future_status::ready,
            timedoutPromise.get_future().wait_for(std::chrono::seconds(3)));
        timer.pleaseStopSync();
    }
}

TEST(aioTimer, cancellation)
{
    const auto timeout = std::chrono::seconds(2);

    nx::utils::promise<void> timedoutPromise;
    auto timeoutHandler = [&timedoutPromise] { timedoutPromise.set_value(); };

    aio::Timer timer;
    timer.start(timeout, timeoutHandler);
    timer.post([&timer]{ timer.cancelSync(); });

    ASSERT_EQ(
        std::future_status::timeout,
        timedoutPromise.get_future().wait_for(timeout*2));
    timer.pleaseStopSync();
}

}   //aio
}   //network
}   //nx
