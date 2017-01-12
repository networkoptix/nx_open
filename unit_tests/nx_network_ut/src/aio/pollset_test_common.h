#pragma once

#include <memory>
#include <vector>

#include <nx/network/system_socket.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/utils/random.h>

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
        registerTest(
            &CommonPollSetTest::removingSocketWithMultipleEvents,
            "removing_socket_with_multiple_events");

        registerTest(
            &CommonPollSetTest::multiplePollsetIterators,
            "multiple_pollset_iterators");
    }

    ~CommonPollSetTest()
    {
    }

    void setPollset(PollsetType* pollset)
    {
        m_pollset = pollset;
    }

    void runTests()
    {
    }

protected:
    virtual void simulateSocketEvents(int events) = 0;

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
        for (const auto& socket : m_sockets)
        {
            if (events & aio::etRead)
                m_pollset->add(socket.get(), aio::etRead);
            if (events & aio::etWrite)
                m_pollset->add(socket.get(), aio::etWrite);
        }
    }

    void unsubscribeSocketFromEvents(Pollable* const socket, int events)
    {
        if (events & aio::etRead)
            m_pollset->remove(socket, aio::etRead);
        if (events & aio::etWrite)
            m_pollset->remove(socket, aio::etWrite);
    }

    template<typename EventHandler>
    void handleSocketEvents(EventHandler handler)
    {
        ASSERT_GT(m_pollset->poll(), 0);

        for (auto it = m_pollset->begin(); it != m_pollset->end(); ++it)
            handler(it.socket(), it.eventType());
    }

    void initializeBunchOfSocketsOfRandomType()
    {
        constexpr int socketCount = 100;

        for (int i = 0; i < socketCount; ++i)
        {
            if (nx::utils::random::number<int>(0, 1) > 0)
                initializeUdtSocket();
            else
                initializeRegularSocket();
        }
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

private:
    PollsetType* m_pollset;
    std::vector<std::unique_ptr<Pollable>> m_sockets;

    void registerTest(TestFuncType testFunc, const char* testName)
    {
        //TODO
    }


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
};

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
