#pragma once

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <nx/network/system_socket.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/utils/random.h>
#include <nx/utils/test_support/utils.h>

namespace nx {
namespace network {
namespace aio {
namespace test {

template<typename PollsetType>
class CommonPollSetTest
{
    using SelfType = CommonPollSetTest<PollsetType>;
    typedef void(SelfType::*TestFuncType)();

public:
    CommonPollSetTest()
    {
    }

    ~CommonPollSetTest()
    {
    }

    void setPollset(PollsetType* pollset)
    {
        m_pollset = pollset;
    }

    template<typename ActualTestClassImplementation>
    static void runTests()
    {
        runTest<ActualTestClassImplementation>(
            &CommonPollSetTest::removingSocketWithMultipleEvents,
            "removing_socket_with_multiple_events");

        runTest<ActualTestClassImplementation>(
            &CommonPollSetTest::multiplePollsetIterators,
            "multiple_pollset_iterators");

        runTest<ActualTestClassImplementation>(
            &CommonPollSetTest::removeSocketThatHasUnprocessedEvents,
            "remove_socket_that_has_unprocessed_events");
    }

    template<typename ActualTestClassImplementation>
    static void runTest(
        typename ActualTestClassImplementation::TestFuncType testFunc,
        const char* testName)
    {
        std::cout << "[      SUB ] " << testName << std::endl;
        ActualTestClassImplementation test;
        (test.*testFunc)();
    }

protected:
    virtual void simulateSocketEvents(int events) = 0;

    virtual std::unique_ptr<Pollable> createRegularSocket()
    {
        auto udpSocket = std::make_unique<UDPSocket>(AF_INET);
        NX_GTEST_ASSERT_TRUE(udpSocket->bind(SocketAddress(HostAddress::localhost, 0)));
        return udpSocket;
    }

    virtual std::unique_ptr<Pollable> createSocketOfRandomType()
    {
        if (nx::utils::random::number<int>(0, 1) > 0)
            return std::make_unique<UdtStreamSocket>(AF_INET);
        else
            return createRegularSocket();
    }

    void initializeSocketOfRandomType()
    {
        m_sockets.push_back(createSocketOfRandomType());
    }

    void initializeUdtSocket()
    {
        m_sockets.push_back(std::make_unique<UdtStreamSocket>(AF_INET));
    }

    void subscribeSocketsToEvents(int events)
    {
        for (const auto& socket: m_sockets)
        {
            if (events & aio::etRead)
                m_pollset->add(socket.get(), aio::etRead);
            if (events & aio::etWrite)
                m_pollset->add(socket.get(), aio::etWrite);
            m_socketToActiveEventMask[socket.get()] = events;
        }
    }

    void unsubscribeSocketFromEvents(Pollable* const socket, int events)
    {
        if ((events & aio::etRead) > 0 && (m_socketToActiveEventMask[socket] & aio::etRead) > 0)
        {
            m_pollset->remove(socket, aio::etRead);
            m_socketToActiveEventMask[socket] &= ~aio::etRead;
        }
        if ((events & aio::etWrite) > 0 && (m_socketToActiveEventMask[socket] & aio::etWrite) > 0)
        {
            m_pollset->remove(socket, aio::etWrite);
            m_socketToActiveEventMask[socket] &= ~aio::etWrite;
        }
    }

    template<typename EventHandler>
    void handleSocketEvents(EventHandler handler)
    {
        ASSERT_GT(m_pollset->poll(), 0);

        for (auto it = m_pollset->begin(); it != m_pollset->end(); ++it)
            handler(it.socket(), it.eventType());
    }

    PollsetType& pollset()
    {
        return *m_pollset;
    }

    const PollsetType& pollset() const
    {
        return *m_pollset;
    }

    std::vector<std::unique_ptr<Pollable>>& sockets()
    {
        return m_sockets;
    }

    //---------------------------------------------------------------------------------------------
    // Tests

    void initializeRegularSocket()
    {
        m_sockets.push_back(createRegularSocket());
    }

    void givenRegularSocketAvailableForReadWrite()
    {
        initializeRegularSocket();
        subscribeSocketsToEvents(aio::etRead | aio::etWrite);
        simulateSocketEvents(aio::etRead | aio::etWrite);
    }

    void initializeBunchOfSocketsOfRandomType()
    {
        constexpr int socketCount = 100;

        for (int i = 0; i < socketCount; ++i)
            initializeSocketOfRandomType();
    }

    void runRemoveSocketWithMultipleEventsTest()
    {
        subscribeSocketsToEvents(aio::etRead | aio::etWrite);
        simulateSocketEvents(aio::etRead | aio::etWrite);
        int numberOfEventsReported = 0;
        handleSocketEvents(
            [&](Pollable* const socket, aio::EventType eventType)
            {
                static_cast<void*>(&eventType);
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

    void whenRemovedSocketFromPollSetOnFirstEvent()
    {
        handleSocketEvents(
            [&](Pollable* const socket, aio::EventType eventType)
            {
                assertIfEventIsNotExpected(socket, eventType);

                unsubscribeSocketFromEvents(socket, aio::etRead | aio::etWrite);
                removeSocket(socket);
            });
    }

    void thenPollsetDidNotReportEventsForRemovedSockets()
    {
        // If we did not crash to this point, then everything is ok.
    }

private:
    PollsetType* m_pollset;
    std::vector<std::unique_ptr<Pollable>> m_sockets;
    std::map<Pollable*, int> m_socketToActiveEventMask;

    //---------------------------------------------------------------------------------------------
    // Tests

    void removingSocketWithMultipleEvents()
    {
        initializeBunchOfSocketsOfRandomType();
        runRemoveSocketWithMultipleEventsTest();
    }

    void multiplePollsetIterators()
    {
        const auto additionalPollsetIteratorInstance = pollset().end();

        initializeRegularSocket();
        runRemoveSocketWithMultipleEventsTest();
    }

    void removeSocketThatHasUnprocessedEvents()
    {
        givenRegularSocketAvailableForReadWrite();
        whenRemovedSocketFromPollSetOnFirstEvent();
        thenPollsetDidNotReportEventsForRemovedSockets();
    }

    // End of tests
    //---------------------------------------------------------------------------------------------

    void assertIfEventIsNotExpected(Pollable* const socket, aio::EventType eventType)
    {
        auto socketIter = std::find_if(
            m_sockets.begin(),
            m_sockets.end(),
            [socket](const std::unique_ptr<Pollable>& element)
            {
                return socket == element.get();
            });
        ASSERT_TRUE(socketIter != m_sockets.end());

        auto socketEventIter = m_socketToActiveEventMask.find(socket);
        ASSERT_TRUE(socketEventIter != m_socketToActiveEventMask.end());
        ASSERT_NE(0, socketEventIter->second & eventType);
    }

    void removeSocket(Pollable* const socket)
    {
        auto socketIter = std::find_if(
            m_sockets.begin(),
            m_sockets.end(),
            [socket](const std::unique_ptr<Pollable>& element)
            {
                return socket == element.get();
            });
        ASSERT_TRUE(socketIter != m_sockets.end());
        m_sockets.erase(socketIter);

        m_socketToActiveEventMask.erase(socket);
    }
};

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
