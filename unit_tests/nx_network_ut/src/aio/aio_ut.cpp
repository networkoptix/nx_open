/**********************************************************
* Nov 12, 2015
* a.kolesnikov
***********************************************************/

#include <functional>

#include <gtest/gtest.h>

#include <nx/network/aio/pollset.h>
#include <nx/network/socket_global.h>
#include <nx/network/system_socket.h>
#include <nx/utils/std/future.h>


namespace nx {
namespace network {
namespace aio {

class DummyEventHandler
:
    public AIOEventHandler<Pollable>
{
public:
    DummyEventHandler(std::function<void(Pollable*, aio::EventType)> func)
    :
        m_func(std::move(func))
    {
    }

    virtual void eventTriggered(Pollable* sock, aio::EventType eventType) throw() override
    {
        m_func(sock, eventType);
    }

private:
    std::function<void(Pollable*, aio::EventType)> m_func;
};


TEST(aio, post)
{
    int x = 0;
    nx::network::SocketGlobals::aioService().post([&x]() { ++x; });
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_EQ(1, x);
}

TEST(aio, socketPolledNotification)
{
    UDPSocket socket(AF_INET);
    std::atomic<bool> handlerCalledFlag(false);
    DummyEventHandler evHandler(
        [&handlerCalledFlag](Pollable*, aio::EventType) {
            handlerCalledFlag = true;
        });
    std::atomic<bool> socketAddedFlag(false);

    aio::AIOThread aioThread;
    aioThread.start();

    aioThread.watchSocket(
        &socket,
        aio::etRead,
        &evHandler,
        std::chrono::milliseconds(0),
        [&socketAddedFlag](){
            socketAddedFlag = true;
        });

    std::this_thread::sleep_for(std::chrono::seconds(2));
    ASSERT_TRUE(socketAddedFlag);
    ASSERT_FALSE(handlerCalledFlag);
    aioThread.removeFromWatch(&socket, aio::etRead, true);
}

class TestPollSet
:
    public aio::PollSet
{
public:
    bool add(
        Pollable* const /*sock*/,
        aio::EventType /*eventType*/,
        void* /*userData*/ = NULL)
    {
        return false;
    }
};

#if 0
//TODO #ak find a way to mock PollSet instance to AioThread
TEST(aio, pollsetError)
{
    nx::utils::promise<void> handlerCalledPromise;
    auto handlerCalledFuture = handlerCalledPromise.get_future();

    UDPSocket socket(false);
    std::atomic<bool> handlerCalledFlag(false);
    std::atomic<bool> socketAddedFlag(false);

    DummyEventHandler evHandler(
        [&handlerCalledFlag, &socketAddedFlag, &handlerCalledPromise](Pollable*, aio::EventType et) {
            ASSERT_EQ(aio::etError, et);
            ASSERT_TRUE(socketAddedFlag.load());
            handlerCalledFlag = true;
            handlerCalledPromise.set_value();
        });

    aio::detail::AIOThread<Pollable, TestPollSet> aioThread;
    aioThread.start();

    aioThread.watchSocket(
        &socket,
        aio::etRead,
        &evHandler,
        std::chrono::milliseconds(0),
        [&socketAddedFlag, &handlerCalledFlag]() {
            ASSERT_FALSE(handlerCalledFlag.load());
            socketAddedFlag = true;
        });

    handlerCalledFuture.wait();
    //handlerCalledFuture.wait_for(std::chrono::seconds(2));
    ASSERT_TRUE(socketAddedFlag);
    ASSERT_TRUE(handlerCalledFlag.load());
}
#endif

}   //aio
}   //network
}   //nx
