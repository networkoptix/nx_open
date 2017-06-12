#include <gtest/gtest.h>

#include <nx/network/aio/aio_thread.h>
#include <nx/network/aio/pollset_wrapper.h>
#include <nx/network/aio/unified_pollset.h>
#include <nx/network/system_socket.h>
#include <nx/network/udt/udt_socket_impl.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/utils/random.h>
#include <nx/utils/std/future.h>
#include <nx/utils/test_support/utils.h>

#include "pollset_test_common.h"

namespace nx {
namespace network {
namespace aio {
namespace test {

constexpr std::size_t kBytesToSend = 128;

class UnifiedPollSet:
    public CommonPollSetTest
{
public:
    UnifiedPollSet()
    {
        init();

        setPollset(&m_pollset);
    }

    ~UnifiedPollSet()
    {
        m_udtServerSocket.pleaseStopSync();
        for (auto& socket: m_acceptedConnections)
            socket->pleaseStopSync();
    }

protected:
    virtual bool simulateSocketEvent(Pollable* socket, int /*eventMask*/) override
    {
        const auto udtSocket = dynamic_cast<UdtStreamSocket*>(socket);
        if (udtSocket)
        {
            // Connect sometimes fails for unknown reason. It is some UDT problem.
            if (!udtSocket->connect(m_udtServerSocket.getLocalAddress()))
                return false;
            char buf[kBytesToSend / 2];
            //const auto localAddress = udtSocket->getLocalAddress();
            //const auto remoteAddress = udtSocket->getForeignAddress();
            NX_GTEST_ASSERT_TRUE(udtSocket->recv(buf, sizeof(buf)) > 0);
        }
        else
        {
            const auto udpSocket = dynamic_cast<UDPSocket*>(socket);
            char buf[16];
            NX_GTEST_ASSERT_TRUE(udpSocket->sendTo(buf, sizeof(buf), udpSocket->getLocalAddress()));
        }

        return true;
    }

private:
    aio::PollSetWrapper<aio::UnifiedPollSet> m_pollset;
    UdtStreamServerSocket m_udtServerSocket;
    std::list<std::unique_ptr<AbstractStreamSocket>> m_acceptedConnections;
    nx::Buffer m_dataToSend;

    void init()
    {
        using namespace std::placeholders;

        m_dataToSend.resize(kBytesToSend);

        ASSERT_TRUE(m_udtServerSocket.setNonBlockingMode(true));
        ASSERT_TRUE(m_udtServerSocket.bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_udtServerSocket.listen());
        m_udtServerSocket.acceptAsync(
            std::bind(&UnifiedPollSet::onConnectionAccepted, this, _1, _2));
    }

    void onConnectionAccepted(
        SystemError::ErrorCode /*sysErrorCode*/,
        std::unique_ptr<AbstractStreamSocket> streamSocket)
    {
        using namespace std::placeholders;

        ASSERT_NE(nullptr, streamSocket);
        ASSERT_TRUE(streamSocket->setNonBlockingMode(true));
        streamSocket->sendAsync(
            m_dataToSend,
            std::bind(&UnifiedPollSet::onBytesSent, this, streamSocket.get(), _1, _2));
        m_acceptedConnections.push_back(std::move(streamSocket));

        m_udtServerSocket.acceptAsync(
            std::bind(&UnifiedPollSet::onConnectionAccepted, this, _1, _2));
    }

    void onBytesSent(
        AbstractStreamSocket* streamSocket,
        SystemError::ErrorCode sysErrorCode,
        size_t /*bytesSent*/)
    {
        using namespace std::placeholders;

        if (sysErrorCode != SystemError::noError)
            return;

        streamSocket->sendAsync(
            m_dataToSend,
            std::bind(&UnifiedPollSet::onBytesSent, this, streamSocket, _1, _2));
    }
};

TEST(UnifiedPollSet, all_tests)
{
    UnifiedPollSet::runTests<UnifiedPollSet>();
}

INSTANTIATE_TYPED_TEST_CASE_P(UnifiedPollSet, PollSetPerformance, aio::UnifiedPollSet);

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
