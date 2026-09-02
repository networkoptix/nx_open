#include <algorithm>
#include <condition_variable>
#include <future>
#include <mutex>
#include <ranges>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <nx/ranges.h>

#include <udt/core.h>
#include <udt/multiplexer.h>

namespace test {

class CSndUList:
    public ::testing::Test
{
public:
    CSndUList()
    {
        m_multiplexer = std::make_shared<Multiplexer>(
            AF_INET,
            /*payloadSize*/ 4*1024,
            /*reuseAddr*/ true,
            /*id*/ 1);
    }

    void createUdt()
    {
        m_u = std::make_shared<CUDT>();
        m_u->open();
        m_u->setMultiplexer(m_multiplexer);
    }

protected:
//private:
    std::shared_ptr<CUDT> m_u;
    std::shared_ptr<Multiplexer> m_multiplexer;
};

TEST_F(CSndUList, no_recursive_lock_when_CSndUList_gets_CUDT_ownership)
{
    constexpr int iteratorCount = 101;

    for (int i = 0; i < iteratorCount; ++i)
    {
        createUdt();
        m_multiplexer->sendQueue().sndUList().update(m_u, true);

        std::promise<void> threadStarted;
        std::thread popThread(
            [this, &threadStarted]()
            {
                threadStarted.set_value();

                detail::SocketAddress addr;
                m_multiplexer->sendQueue().sndUList().pop(addr);
            });

        threadStarted.get_future().wait();

        // NOTE: Trying to achieve the situation when sndUList().pop() owns the m_u object.
        // (it locks weak_ptr inside).
        m_u.reset();

        popThread.join();
    }
}

//-------------------------------------------------------------------------------------------------

// The parameter is the number of sockets scheduled on the list. Every scheduled socket occupies
// one entry of the heap array, so a count above the capacity that array was created with is only
// valid if the array grows.
using SendListCapacity = ::testing::TestWithParam<int>;

// A socket offered once the heap array is full is refused, so the array is never indexed past
// its end and the list never grows to take memory from the rest of the process.
TEST_P(SendListCapacity, sockets_are_scheduled_up_to_the_capacity)
{
    const auto multiplexer = std::make_shared<Multiplexer>(
        AF_INET,
        /*maximumSegmentSize*/ UDT::kMSSMax,
        /*reusable*/ true,
        /*id*/ 1);

    const std::vector sockets = std::views::iota(0, GetParam())
        | std::views::transform(
            [&multiplexer](int) -> std::shared_ptr<CUDT>
            {
                auto socket = std::make_shared<CUDT>();
                socket->open();
                socket->setMultiplexer(multiplexer);
                return socket;
            })
        | nx::ranges::to<std::vector>();

    const int capacity = multiplexer->sendQueue().sndUList().heapCapacity();

    int refused = 0;
    for (const auto& socket: sockets)
    {
        if (!multiplexer->sendQueue().sndUList().update(socket, /*reschedule*/ true).ok())
            ++refused;
    }

    EXPECT_EQ(std::max(GetParam() - capacity, 0), refused);

    // lastEntry() is the index of the last occupied entry, and -1 when the list is empty.
    EXPECT_EQ(
        std::min(GetParam(), capacity) - 1,
        multiplexer->sendQueue().sndUList().lastEntry());

    for (const auto& socket: sockets)
        multiplexer->sendQueue().sndUList().remove(socket.get());

    EXPECT_EQ(-1, multiplexer->sendQueue().sndUList().lastEntry());
}

INSTANTIATE_TEST_SUITE_P(
    /*prefix*/ ,
    SendListCapacity,
    ::testing::Values(
        ::CSndUList::kInitialHeapCapacity - 1,
        ::CSndUList::kInitialHeapCapacity,
        ::CSndUList::kInitialHeapCapacity + 1),
    /*nameGenerator*/
    [](const ::testing::TestParamInfo<int>& info) -> std::string
    {
        if (info.param < ::CSndUList::kInitialHeapCapacity)
            return "BelowInitialCapacity";

        return info.param == ::CSndUList::kInitialHeapCapacity
            ? "AtInitialCapacity"
            : "AboveInitialCapacity";
    });

} // namespace
