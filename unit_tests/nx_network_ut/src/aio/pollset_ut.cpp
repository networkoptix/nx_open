#include <gtest/gtest.h>

#include <nx/network/aio/pollset.h>
#include <nx/network/aio/pollset_wrapper.h>
#include <nx/utils/test_support/utils.h>

#include "pollset_test_common.h"

namespace nx {
namespace network {
namespace aio {
namespace test {

class PollSet:
    public CommonPollSetTest
{
public:
    PollSet()
    {
        setPollset(&m_pollset);
    }

protected:
    virtual bool simulateSocketEvent(Pollable* socket, int /*eventMask*/) override
    {
        const auto udpSocket = static_cast<UDPSocket*>(socket);

        char buf[16];
        NX_GTEST_ASSERT_TRUE(udpSocket->sendTo(buf, sizeof(buf), udpSocket->getLocalAddress()));

        return true;
    }

    virtual std::unique_ptr<Pollable> createSocketOfRandomType() override
    {
        return createRegularSocket();
    }

    virtual std::vector<std::unique_ptr<Pollable>> createSocketOfAllSupportedTypes() override
    {
        std::vector<std::unique_ptr<Pollable>> sockets;
        sockets.push_back(createRegularSocket());
        return sockets;
    }

private:
    aio::PollSetWrapper<aio::PollSet> m_pollset;
};

TEST(PollSet, all_tests)
{
    PollSet::runTests<PollSet>();
}

INSTANTIATE_TYPED_TEST_CASE_P(PollSet, PollSetPerformance, aio::PollSet);

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
