/**********************************************************
* Nov 12, 2015
* a.kolesnikov
***********************************************************/

#include <functional>

#include <gtest/gtest.h>

#include <utils/network/aio/pollset.h>
#include <utils/network/socket_global.h>
#include <utils/network/system_socket.h>


class DummyEventHandler
:
    public aio::AIOEventHandler<Pollable>
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
    nx::SocketGlobals::aioService().post([&x]() { ++x; });
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_EQ(1, x);
}

TEST(aio, socketPolledNotification)
{
    UDPSocket socket(false);
    std::atomic<bool> handlerCalledFlag(false);
    DummyEventHandler evHandler(
        [&handlerCalledFlag](Pollable*, aio::EventType) {
            handlerCalledFlag = true;
        });
    std::atomic<bool> socketAddedFlag(false);

    QnMutex mtx;
    aio::AIOThread<Pollable> aioThread(&mtx);
    aioThread.start();

    aioThread.watchSocket(
        socket.implementationDelegate(),
        aio::etRead,
        &evHandler,
        0,
        [&socketAddedFlag](){
            socketAddedFlag = true;
        });

    std::this_thread::sleep_for(std::chrono::seconds(2));
    ASSERT_TRUE(socketAddedFlag);
    ASSERT_FALSE(handlerCalledFlag);
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

TEST(aio, DISABLED_pollsetError)
{
    UDPSocket socket(false);
    std::atomic<bool> handlerCalledFlag(false);
    DummyEventHandler evHandler(
        [&handlerCalledFlag](Pollable*, aio::EventType et) {
            ASSERT_EQ(aio::etError, et);
            handlerCalledFlag = true;
        });
    std::atomic<bool> socketAddedFlag(false);

    QnMutex mtx;
    aio::detail::AIOThread<Pollable, TestPollSet> aioThread(&mtx);
    aioThread.start();

    aioThread.watchSocket(
        socket.implementationDelegate(),
        aio::etRead,
        &evHandler,
        0,
        [&socketAddedFlag]() {
            socketAddedFlag = true;
        });

    std::this_thread::sleep_for(std::chrono::seconds(2));
    ASSERT_FALSE(socketAddedFlag);
    ASSERT_TRUE(handlerCalledFlag);
}
