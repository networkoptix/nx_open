#include <gtest/gtest.h>

#include <nx/network/aio/pollset.h>
#include <nx/network/aio/pollset_wrapper.h>
#include <nx/utils/test_support/utils.h>

#include "pollset_test_common.h"
#include "pollset_performance_tests.h"

namespace nx {
namespace network {
namespace aio {
namespace test {

class PollSetHelper
{
public:
    using PollSet = aio::PollSetWrapper<aio::PollSet>;

    bool simulateSocketEvent(Pollable* socket, int /*eventMask*/)
    {
        const auto udpSocket = static_cast<UDPSocket*>(socket);

        char buf[16];
        NX_GTEST_ASSERT_TRUE(udpSocket->sendTo(buf, sizeof(buf), udpSocket->getLocalAddress()));

        return true;
    }

    std::unique_ptr<Pollable> createRegularSocket()
    {
        auto udpSocket = std::make_unique<UDPSocket>(AF_INET);
        NX_GTEST_ASSERT_TRUE(udpSocket->bind(SocketAddress(HostAddress::localhost, 0)));
        return std::move(udpSocket);
    }

    std::unique_ptr<Pollable> createSocketOfRandomType()
    {
        return createRegularSocket();
    }

    std::vector<std::unique_ptr<Pollable>> createSocketOfAllSupportedTypes()
    {
        std::vector<std::unique_ptr<Pollable>> sockets;
        sockets.push_back(createRegularSocket());
        return sockets;
    }
};

INSTANTIATE_TYPED_TEST_CASE_P(PollSet, PollSetAcceptance, PollSetHelper);

//-------------------------------------------------------------------------------------------------

INSTANTIATE_TYPED_TEST_CASE_P(PollSet, PollSetPerformance, aio::PollSet);

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
