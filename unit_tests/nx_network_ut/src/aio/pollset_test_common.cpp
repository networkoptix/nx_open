#include "pollset_test_common.h"

namespace nx {
namespace network {
namespace aio {
namespace test {

void CommonPollSetTest::setPollset(AbstractPollSet* pollset)
{
    m_pollset = pollset;
}

std::unique_ptr<Pollable> CommonPollSetTest::createRegularSocket()
{
    auto udpSocket = std::make_unique<UDPSocket>(AF_INET);
    NX_GTEST_ASSERT_TRUE(udpSocket->bind(SocketAddress(HostAddress::localhost, 0)));
    return std::move(udpSocket);
}

std::unique_ptr<Pollable> CommonPollSetTest::createSocketOfRandomType()
{
    if (nx::utils::random::number<int>(0, 1) > 0)
        return std::make_unique<UdtStreamSocket>(AF_INET);
    else
        return createRegularSocket();
}

std::vector<std::unique_ptr<Pollable>> CommonPollSetTest::createSocketOfAllSupportedTypes()
{
    std::vector<std::unique_ptr<Pollable>> sockets;
    sockets.push_back(createRegularSocket());
    sockets.push_back(std::make_unique<UdtStreamSocket>(AF_INET));
    return sockets;
}

void CommonPollSetTest::initializeSocketOfRandomType()
{
    m_sockets.push_back(createSocketOfRandomType());
}

void CommonPollSetTest::initializeUdtSocket()
{
    m_sockets.push_back(std::make_unique<UdtStreamSocket>(AF_INET));
}

void CommonPollSetTest::subscribeSocketsToEvents(int events)
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

void CommonPollSetTest::unsubscribeSocketFromEvents(Pollable* const socket, int events)
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
void CommonPollSetTest::handleSocketEvents(EventHandler handler)
{
    ASSERT_GT(m_pollset->poll(), 0);

    auto it = m_pollset->getSocketEventsIterator();
    while (it->next())
        handler(it->socket(), it->eventReceived());
}

std::vector<std::unique_ptr<Pollable>>& CommonPollSetTest::sockets()
{
    return m_sockets;
}

//---------------------------------------------------------------------------------------------
// Tests

void CommonPollSetTest::initializeRegularSocket()
{
    m_sockets.push_back(createRegularSocket());
}

void CommonPollSetTest::initializeBunchOfSocketsOfRandomType()
{
    constexpr int socketCount = 100;

    for (int i = 0; i < socketCount; ++i)
        initializeSocketOfRandomType();
}

void CommonPollSetTest::givenRegularSocketSubscribedToReadWriteEventPolling()
{
    initializeRegularSocket();
    subscribeSocketsToEvents(aio::etRead | aio::etWrite);
}

void CommonPollSetTest::givenRegularSocketAvailableForReadWrite()
{
    givenRegularSocketSubscribedToReadWriteEventPolling();
    simulateSocketEvents(aio::etRead | aio::etWrite);
}

void CommonPollSetTest::givenSocketsOfAllSupportedTypes()
{
    m_sockets = createSocketOfAllSupportedTypes();
    subscribeSocketsToEvents(aio::etRead | aio::etWrite);
}

void CommonPollSetTest::runRemoveSocketWithMultipleEventsTest()
{
    subscribeSocketsToEvents(aio::etRead | aio::etWrite);
    simulateSocketEvents(aio::etRead | aio::etWrite);
    int numberOfEventsReported = 0;
    handleSocketEvents(
        [this, &numberOfEventsReported](Pollable* const socket, aio::EventType /*eventType*/)
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

void CommonPollSetTest::whenMadeSocketAvailableForReadWrite()
{
    simulateSocketEvents(aio::etRead | aio::etWrite);
}

void CommonPollSetTest::whenRemovedSocketFromPollSetOnFirstEvent()
{
    handleSocketEvents(
        [this](Pollable* const socket, aio::EventType eventType)
        {
            assertIfEventIsNotExpected(socket, eventType);

            unsubscribeSocketFromEvents(socket, aio::etRead | aio::etWrite);
            removeSocket(socket);
        });
}

void CommonPollSetTest::whenReceivedSocketEvents()
{
    handleSocketEvents(
        [this](Pollable* const socket, aio::EventType eventType)
        {
            m_eventsReported.emplace(socket, eventType);
        });
}

void CommonPollSetTest::whenChangedEverySocketState()
{
    simulateSocketEvents(aio::etRead | aio::etWrite);
}

void CommonPollSetTest::thenPollsetDidNotReportEventsForRemovedSockets()
{
    // If we did not crash up to this point, then everything is ok.
}

void CommonPollSetTest::thenReadWriteEventsShouldBeReportedEventually()
{
    for (;;)
    {
        whenReceivedSocketEvents();
        if (isEveryExpectedEventHasBeenReported({ aio::etRead, aio::etWrite }))
            break;
        m_eventsReported.clear();
    }
}

void CommonPollSetTest::thenPollsetReportsSocketsAsSignalledMultipleTimes()
{
    constexpr int numberOfChecks = 2;

    for (int i = 0; i < numberOfChecks; ++i)
    {
        thenReadWriteEventsShouldBeReportedEventually();
        m_eventsReported.clear();
    }
}

//---------------------------------------------------------------------------------------------
// Tests

void CommonPollSetTest::allEventsAreActuallyReported()
{
    givenRegularSocketSubscribedToReadWriteEventPolling();
    whenMadeSocketAvailableForReadWrite();
    thenReadWriteEventsShouldBeReportedEventually();
}

void CommonPollSetTest::removingSocketWithMultipleEvents()
{
    initializeBunchOfSocketsOfRandomType();
    runRemoveSocketWithMultipleEventsTest();
}

void CommonPollSetTest::multiplePollsetIterators()
{
    const auto additionalPollsetIteratorInstance = m_pollset->getSocketEventsIterator();

    initializeRegularSocket();
    runRemoveSocketWithMultipleEventsTest();
}

void CommonPollSetTest::removeSocketThatHasUnprocessedEvents()
{
    givenRegularSocketAvailableForReadWrite();
    whenRemovedSocketFromPollSetOnFirstEvent();
    thenPollsetDidNotReportEventsForRemovedSockets();
}

void CommonPollSetTest::pollIsActuallyLevelTriggered()
{
    givenSocketsOfAllSupportedTypes();
    whenChangedEverySocketState();
    thenPollsetReportsSocketsAsSignalledMultipleTimes();
}

// End of tests
//---------------------------------------------------------------------------------------------

void CommonPollSetTest::simulateSocketEvents(int eventMask)
{
    for (auto it = m_sockets.begin(); it != m_sockets.end();)
    {
        if (simulateSocketEvent(it->get(), eventMask))
        {
            ++it;
        }
        else
        {
            unsubscribeSocketFromEvents(it->get(), aio::etRead | aio::etWrite);
            it = m_sockets.erase(it);
        }
    }

    ASSERT_FALSE(m_sockets.empty());
}

void CommonPollSetTest::assertIfEventIsNotExpected(
    Pollable* const socket, aio::EventType eventType)
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

void CommonPollSetTest::removeSocket(Pollable* const socket)
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

bool CommonPollSetTest::isEveryExpectedEventHasBeenReported(
    std::vector<aio::EventType> expectedEvents) const
{
    for (const auto& socketAndEventMask: m_socketToActiveEventMask)
    {
        for (auto eventTypeToCheck: expectedEvents)
        {
            if ((socketAndEventMask.second & eventTypeToCheck) == 0)
                continue;
            auto reportedEventIter =
                m_eventsReported.find(std::make_pair(socketAndEventMask.first, eventTypeToCheck));
            if (reportedEventIter == m_eventsReported.end())
                return false;
        }
    }

    return true;
}

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
