#include <gtest/gtest.h>

#include <nx/network/aio/pollset.h>

#include "pollset_test_common.h"

namespace nx {
namespace network {
namespace aio {
namespace test {

class PollSet:
    public CommonPollSetTest<aio::PollSet>,
    public ::testing::Test
{
public:
    PollSet()
    {
        setPollset(&m_pollset);
    }

    ~PollSet()
    {
    }

protected:
    virtual void simulateSocketEvents(int events) override
    {
        // TODO
        //for (const auto& socket : sockets())
        //{
        //    if (events & aio::etRead)
        //        m_epollWrapperPtr->markAsReadable(socket.get());
        //    if (events & aio::etWrite)
        //        m_epollWrapperPtr->markAsWritable(socket.get());
        //}
    }

private:
    aio::PollSet m_pollset;
};

TEST_F(PollSet, removing_socket_with_multiple_events)
{
    initializeBunchOfSocketsOfRandomType();
    runRemoveSocketWithMultipleEventsTest();
}

TEST_F(PollSet, multiple_pollset_iterators)
{
    const auto additionalPollsetIteratorInstance = pollset().end();

    initializeRegularSocket();
    runRemoveSocketWithMultipleEventsTest();
}

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
