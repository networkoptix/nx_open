#include <gtest/gtest.h>

#include <nx/network/aio/aiothread.h>
#include <nx/network/aio/unified_pollset.h>
#include <nx/network/system_socket.h>
#include <nx/network/udt/udt_socket_impl.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/utils/random.h>
#include <nx/utils/std/future.h>

namespace nx {
namespace network {
namespace aio {
namespace test {

class TestUdtEpollWrapper:
    public UdtEpollWrapper
{
public:
    virtual int epollWait(
        int /*epollFd*/,
        std::map<UDTSOCKET, int>* readReadyUdtSockets,
        std::map<UDTSOCKET, int>* writeReadyUdtSockets,
        int64_t /*timeoutMillis*/,
        std::map<AbstractSocket::SOCKET_HANDLE, int>* readReadySystemSockets,
        std::map<AbstractSocket::SOCKET_HANDLE, int>* writeReadySystemSockets) override
    {
        std::size_t numberOfEvents = 0;

        if (readReadySystemSockets)
        {
            numberOfEvents += m_readableSystemSockets.size();
            *readReadySystemSockets = m_readableSystemSockets;
        }

        if (writeReadySystemSockets)
        {
            numberOfEvents += m_writableSystemSockets.size();
            *writeReadySystemSockets = m_writableSystemSockets;
        }

        if (readReadyUdtSockets)
        {
            numberOfEvents += m_readableUdtSockets.size();
            *readReadyUdtSockets = m_readableUdtSockets;
        }

        if (writeReadyUdtSockets)
        {
            numberOfEvents += m_writableUdtSockets.size();
            *writeReadyUdtSockets = m_writableUdtSockets;
        }

        return (int)numberOfEvents;
    }

    void markAsReadable(Pollable* const socket)
    {
        if (socket->impl()->isUdtSocket)
        {
            m_readableUdtSockets.emplace(
                static_cast<UDTSocketImpl*>(socket->impl())->udtHandle, 0);
        }
        else
        {
            m_readableSystemSockets.emplace(socket->handle(), 0);
        }
    }

    void markAsWritable(Pollable* const socket)
    {
        if (socket->impl()->isUdtSocket)
        {
            m_writableUdtSockets.emplace(
                static_cast<UDTSocketImpl*>(socket->impl())->udtHandle, 0);
        }
        else
        {
            m_writableSystemSockets.emplace(socket->handle(), 0);
        }
    }

private:
    std::map<AbstractSocket::SOCKET_HANDLE, int> m_readableSystemSockets;
    std::map<AbstractSocket::SOCKET_HANDLE, int> m_writableSystemSockets;

    std::map<UDTSOCKET, int> m_readableUdtSockets;
    std::map<UDTSOCKET, int> m_writableUdtSockets;
};

class UnifiedPollSet:
    public ::testing::Test
{
public:
    UnifiedPollSet():
        m_epollWrapper(std::make_unique<TestUdtEpollWrapper>()),
        m_epollWrapperPtr(m_epollWrapper.get()),
        m_pollset(std::move(m_epollWrapper))
    {
    }

    ~UnifiedPollSet()
    {
    }

protected:
    void initializeRegularSocket()
    {
        m_sockets.push_back(std::make_unique<UDPSocket>(AF_INET));
    }

    void initializeUdtSocket()
    {
        m_sockets.push_back(std::make_unique<UdtStreamSocket>(AF_INET));
    }

    void subscribeSocketToEvents(int events)
    {
        for (const auto& socket: m_sockets)
        {
            if (events & aio::etRead)
                m_pollset.add(socket.get(), aio::etRead);
            if (events & aio::etWrite)
                m_pollset.add(socket.get(), aio::etWrite);
        }
    }

    void unsubscribeSocketFromEvents(Pollable* const socket, int events)
    {
        if (events & aio::etRead)
            m_pollset.remove(socket, aio::etRead);
        if (events & aio::etWrite)
            m_pollset.remove(socket, aio::etWrite);
    }

    void simulateSocketEvents(int events)
    {
        for (const auto& socket : m_sockets)
        {
            if (events & aio::etRead)
                m_epollWrapperPtr->markAsReadable(socket.get());
            if (events & aio::etWrite)
                m_epollWrapperPtr->markAsWritable(socket.get());
        }
    }

    template<typename EventHandler>
    void handleSocketEvents(EventHandler handler)
    {
        ASSERT_GT(m_pollset.poll(), 0);

        for (auto it = m_pollset.begin(); it != m_pollset.end(); ++it)
            handler(it.socket(), it.eventType());
    }

    void runRemoveSocketWithMultipleEventsTest()
    {
        subscribeSocketToEvents(aio::etRead | aio::etWrite);
        simulateSocketEvents(aio::etRead | aio::etWrite);
        int numberOfEventsReported = 0;
        handleSocketEvents(
            [&](Pollable* const socket, aio::EventType /*eventType*/)
            {
                ++numberOfEventsReported;

                const int seed = nx::utils::random::number<int>(0, 2);
                const auto eventsToRemove = 
                    seed == 0 ? (aio::etRead | aio::etWrite) :
                    seed == 1 ? (aio::etRead) :
                    seed == 2 ? (aio::etWrite) : 0;

                unsubscribeSocketFromEvents(socket, eventsToRemove);
                unsubscribeSocketFromEvents(
                    m_sockets[nx::utils::random::number<size_t>(0, m_sockets.size() - 1)].get(),
                    eventsToRemove);
            });
    }

    aio::UnifiedPollSet& pollset()
    {
        return m_pollset;
    }

    const aio::UnifiedPollSet& pollset() const
    {
        return m_pollset;
    }

private:
    std::unique_ptr<TestUdtEpollWrapper> m_epollWrapper;
    TestUdtEpollWrapper* m_epollWrapperPtr;
    aio::UnifiedPollSet m_pollset;
    std::vector<std::unique_ptr<Pollable>> m_sockets;
};

TEST_F(UnifiedPollSet, removing_socket_with_multiple_events)
{
    constexpr int socketCount = 100;

    for (int i = 0; i < socketCount; ++i)
    {
        if (nx::utils::random::number<int>(0, 1) > 0)
            initializeUdtSocket();
        else
            initializeRegularSocket();
    }

    runRemoveSocketWithMultipleEventsTest();
}

TEST_F(UnifiedPollSet, multiple_pollset_iterators)
{
    const auto it = pollset().end();

    initializeRegularSocket();
    runRemoveSocketWithMultipleEventsTest();
}

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
