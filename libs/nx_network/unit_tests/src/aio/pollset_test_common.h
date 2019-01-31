#pragma once

#include <memory>
#include <set>
#include <vector>

#include <gtest/gtest.h>

#include <nx/network/aio/abstract_pollset.h>
#include <nx/network/system_socket.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/utils/random.h>
#include <nx/utils/test_support/utils.h>

namespace nx {
namespace network {
namespace aio {
namespace test {

template<typename PollsetHelperType>
class PollSetAcceptance:
    public ::testing::Test
{
protected:
    void initializeSocketOfRandomType();
    void initializeUdtSocket();
    void subscribeSocketsToEvents(int events);
    void unsubscribeSocketFromEvents(Pollable* const socket, int events);

    template<typename EventHandler>
    void handleSocketEvents(EventHandler handler);

    std::vector<std::unique_ptr<Pollable>>& sockets();

    //---------------------------------------------------------------------------------------------
    // Tests

    void initializeRegularSocket();
    void initializeBunchOfSocketsOfRandomType();

    void givenRegularSocketSubscribedToReadWriteEventPolling();
    void givenRegularSocketAvailableForReadWrite();
    void givenSocketsOfAllSupportedTypes();

    void runRemoveSocketWithMultipleEventsTest();

    void whenMadeSocketAvailableForReadWrite();
    void whenRemovedSocketFromPollSetOnFirstEvent();
    void whenReceivedSocketEvents();
    void whenChangedEverySocketState();

    void thenPollsetDidNotReportEventsForRemovedSockets();
    void thenReadWriteEventsShouldBeReportedEventually();
    void thenPollsetReportsSocketsAsSignalledMultipleTimes();

    typename PollsetHelperType::PollSet m_pollset;

private:
    PollsetHelperType m_pollsetHelper;
    std::vector<std::unique_ptr<Pollable>> m_sockets;
    std::map<Pollable*, int> m_socketToActiveEventMask;
    std::set<std::pair<Pollable*, aio::EventType>> m_eventsReported;

    //---------------------------------------------------------------------------------------------
    // Tests

    void allEventsAreActuallyReported();
    void removingSocketWithMultipleEvents();
    void multiplePollsetIterators();
    void removeSocketThatHasUnprocessedEvents();
    void pollIsActuallyLevelTriggered();

    // End of tests
    //---------------------------------------------------------------------------------------------

    void simulateSocketEvents(int events);
    void assertIfEventIsNotExpected(Pollable* const socket, aio::EventType eventType);
    void removeSocket(Pollable* const socket);
    bool isEveryExpectedEventHasBeenReported(std::vector<aio::EventType> expectedEvents) const;
};

//-------------------------------------------------------------------------------------------------
// PollSetAcceptance implementation.

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::initializeSocketOfRandomType()
{
    m_sockets.push_back(m_pollsetHelper.createSocketOfRandomType());
}

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::initializeUdtSocket()
{
    m_sockets.push_back(std::make_unique<UdtStreamSocket>(AF_INET));
}

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::subscribeSocketsToEvents(int events)
{
    for (const auto& socket: m_sockets)
    {
        if (events & aio::etRead)
            m_pollset.add(socket.get(), aio::etRead, nullptr);
        if (events & aio::etWrite)
            m_pollset.add(socket.get(), aio::etWrite, nullptr);
        m_socketToActiveEventMask[socket.get()] = events;
    }
}

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::unsubscribeSocketFromEvents(
    Pollable* const socket,
    int events)
{
    if ((events & aio::etRead) > 0 && (m_socketToActiveEventMask[socket] & aio::etRead) > 0)
    {
        m_pollset.remove(socket, aio::etRead);
        m_socketToActiveEventMask[socket] &= ~aio::etRead;
    }
    if ((events & aio::etWrite) > 0 && (m_socketToActiveEventMask[socket] & aio::etWrite) > 0)
    {
        m_pollset.remove(socket, aio::etWrite);
        m_socketToActiveEventMask[socket] &= ~aio::etWrite;
    }
}

template<typename PollsetHelperType>
template<typename EventHandler>
void PollSetAcceptance<PollsetHelperType>::handleSocketEvents(EventHandler handler)
{
    ASSERT_GT(m_pollset.poll(), 0);

    auto it = m_pollset.getSocketEventsIterator();
    while (it->next())
        handler(it->socket(), it->eventReceived());
}

template<typename PollsetHelperType>
std::vector<std::unique_ptr<Pollable>>& PollSetAcceptance<PollsetHelperType>::sockets()
{
    return m_sockets;
}

//---------------------------------------------------------------------------------------------
// Tests

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::initializeRegularSocket()
{
    m_sockets.push_back(m_pollsetHelper.createRegularSocket());
}

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::initializeBunchOfSocketsOfRandomType()
{
    constexpr int socketCount = 100;

    for (int i = 0; i < socketCount; ++i)
        initializeSocketOfRandomType();
}

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::givenRegularSocketSubscribedToReadWriteEventPolling()
{
    initializeRegularSocket();
    subscribeSocketsToEvents(aio::etRead | aio::etWrite);
}

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::givenRegularSocketAvailableForReadWrite()
{
    givenRegularSocketSubscribedToReadWriteEventPolling();
    simulateSocketEvents(aio::etRead | aio::etWrite);
}

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::givenSocketsOfAllSupportedTypes()
{
    m_sockets = m_pollsetHelper.createSocketOfAllSupportedTypes();
    subscribeSocketsToEvents(aio::etRead | aio::etWrite);
}

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::runRemoveSocketWithMultipleEventsTest()
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

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::whenMadeSocketAvailableForReadWrite()
{
    simulateSocketEvents(aio::etRead | aio::etWrite);
}

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::whenRemovedSocketFromPollSetOnFirstEvent()
{
    handleSocketEvents(
        [this](Pollable* const socket, aio::EventType eventType)
        {
            assertIfEventIsNotExpected(socket, eventType);

            unsubscribeSocketFromEvents(socket, aio::etRead | aio::etWrite);
            removeSocket(socket);
        });
}

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::whenReceivedSocketEvents()
{
    handleSocketEvents(
        [this](Pollable* const socket, aio::EventType eventType)
        {
            m_eventsReported.emplace(socket, eventType);
        });
}

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::whenChangedEverySocketState()
{
    simulateSocketEvents(aio::etRead | aio::etWrite);
}

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::thenPollsetDidNotReportEventsForRemovedSockets()
{
    // If we did not crash up to this point, then everything is ok.
}

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::thenReadWriteEventsShouldBeReportedEventually()
{
    for (;;)
    {
        whenReceivedSocketEvents();
        if (isEveryExpectedEventHasBeenReported({ aio::etRead, aio::etWrite }))
            break;
        m_eventsReported.clear();
    }
}

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::thenPollsetReportsSocketsAsSignalledMultipleTimes()
{
    constexpr int numberOfChecks = 2;

    for (int i = 0; i < numberOfChecks; ++i)
    {
        thenReadWriteEventsShouldBeReportedEventually();
        m_eventsReported.clear();
    }
}

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::simulateSocketEvents(int eventMask)
{
    for (auto it = m_sockets.begin(); it != m_sockets.end();)
    {
        if (m_pollsetHelper.simulateSocketEvent(it->get(), eventMask))
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

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::assertIfEventIsNotExpected(
    Pollable* const socket,
    aio::EventType eventType)
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

template<typename PollsetHelperType>
void PollSetAcceptance<PollsetHelperType>::removeSocket(Pollable* const socket)
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

template<typename PollsetHelperType>
bool PollSetAcceptance<PollsetHelperType>::isEveryExpectedEventHasBeenReported(
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

TYPED_TEST_CASE_P(PollSetAcceptance);

//-------------------------------------------------------------------------------------------------

TYPED_TEST_P(PollSetAcceptance, allEventsAreActuallyReported)
{
    this->givenRegularSocketSubscribedToReadWriteEventPolling();
    this->whenMadeSocketAvailableForReadWrite();
    this->thenReadWriteEventsShouldBeReportedEventually();
}

TYPED_TEST_P(PollSetAcceptance, removingSocketWithMultipleEvents)
{
    this->initializeBunchOfSocketsOfRandomType();
    this->runRemoveSocketWithMultipleEventsTest();
}

TYPED_TEST_P(PollSetAcceptance, multiplePollsetIterators)
{
    const auto additionalPollsetIteratorInstance =
        this->m_pollset.getSocketEventsIterator();

    this->initializeRegularSocket();
    this->runRemoveSocketWithMultipleEventsTest();
}

TYPED_TEST_P(PollSetAcceptance, removeSocketThatHasUnprocessedEvents)
{
    this->givenRegularSocketAvailableForReadWrite();
    this->whenRemovedSocketFromPollSetOnFirstEvent();
    this->thenPollsetDidNotReportEventsForRemovedSockets();
}

TYPED_TEST_P(PollSetAcceptance, pollIsActuallyLevelTriggered)
{
    this->givenSocketsOfAllSupportedTypes();
    this->whenChangedEverySocketState();
    this->thenPollsetReportsSocketsAsSignalledMultipleTimes();
}

//-------------------------------------------------------------------------------------------------

REGISTER_TYPED_TEST_CASE_P(PollSetAcceptance,
    allEventsAreActuallyReported,
    removingSocketWithMultipleEvents,
    multiplePollsetIterators,
    removeSocketThatHasUnprocessedEvents,
    pollIsActuallyLevelTriggered);

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
