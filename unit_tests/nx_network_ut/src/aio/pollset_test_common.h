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

class CommonPollSetTest
{
    using SelfType = CommonPollSetTest;
    typedef void(SelfType::*TestFuncType)();

public:
    void setPollset(AbstractPollSet* pollset);

    template<typename ActualTestClassImplementation>
    static void runTests()
    {
        runTest<ActualTestClassImplementation>(
            &CommonPollSetTest::allEventsAreActuallyReported,
            "all_events_are_actually_reported");

        runTest<ActualTestClassImplementation>(
            &CommonPollSetTest::removingSocketWithMultipleEvents,
            "removing_socket_with_multiple_events");

        runTest<ActualTestClassImplementation>(
            &CommonPollSetTest::multiplePollsetIterators,
            "multiple_pollset_iterators");

        runTest<ActualTestClassImplementation>(
            &CommonPollSetTest::removeSocketThatHasUnprocessedEvents,
            "remove_socket_that_has_unprocessed_events");

        runTest<ActualTestClassImplementation>(
            &CommonPollSetTest::pollIsActuallyLevelTriggered,
            "poll_is_actually_level_triggered");
    }

    template<typename ActualTestClassImplementation>
    static void runTest(
        typename ActualTestClassImplementation::TestFuncType testFunc,
        const char* testName)
    {
        std::cout << "[  SUBTEST ] " << testName << std::endl;
        ActualTestClassImplementation test;
        (test.*testFunc)();
    }

protected:
    virtual bool simulateSocketEvent(Pollable* socket, int eventMask) = 0;

    virtual std::unique_ptr<Pollable> createRegularSocket();
    virtual std::unique_ptr<Pollable> createSocketOfRandomType();
    virtual std::vector<std::unique_ptr<Pollable>> createSocketOfAllSupportedTypes();

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

private:
    AbstractPollSet* m_pollset;
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

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
