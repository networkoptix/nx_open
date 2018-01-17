#include <gtest/gtest.h>

#include <nx/network/aio/aio_thread.h>
#include <nx/network/aio/pollset_wrapper.h>
#include <nx/network/aio/unified_pollset.h>
#include <nx/network/system_socket.h>
#include <nx/network/udt/udt_socket_impl.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/network/http/test_http_server.h>
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
            if (!udtSocket->connect(m_udtServerSocket.getLocalAddress(), nx::network::kNoTimeout))
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

//-------------------------------------------------------------------------------------------------

class UdtEpollDummyWrapper:
    public AbstractUdtEpollWrapper
{
public:
    std::map<AbstractSocket::SOCKET_HANDLE, int> readEvents;
    std::map<AbstractSocket::SOCKET_HANDLE, int> writeEvents;

    virtual int epollWait(
        int /*epollFd*/,
        std::map<UDTSOCKET, int>* /*readReadyUdtSockets*/,
        std::map<UDTSOCKET, int>* /*writeReadyUdtSockets*/,
        int64_t /*timeoutMillis*/,
        std::map<AbstractSocket::SOCKET_HANDLE, int>* readReadySystemSockets,
        std::map<AbstractSocket::SOCKET_HANDLE, int>* writeReadySystemSockets) override
    {
        *readReadySystemSockets = readEvents;
        *writeReadySystemSockets = writeEvents;
        return (int)(readReadySystemSockets->size() + writeReadySystemSockets->size());
    }

    std::size_t emulatedEventCount() const
    {
        return readEvents.size() + writeEvents.size();
    }
};

class UnifiedPollSetSpecificTests:
    public ::testing::Test
{
public:
    UnifiedPollSetSpecificTests()
    {
        auto udtEpollDummyWrapper = std::make_unique<UdtEpollDummyWrapper>();
        m_udtEpollDummyWrapper = udtEpollDummyWrapper.get();
        m_pollset = std::make_unique<aio::UnifiedPollSet>(std::move(udtEpollDummyWrapper));
    }

protected:
    void initializeMultipleSockets(int socketCount)
    {
        for (int i = 0; i < socketCount; ++i)
        {
            m_sockets.push_back(std::make_unique<TCPSocket>(AF_INET));
            ASSERT_TRUE(m_pollset->add(m_sockets.back().get(), aio::EventType::etRead));
        }
    }

    void emulateSocketReadEvent(SYSSOCKET handle, int eventMask)
    {
        m_udtEpollDummyWrapper->readEvents.emplace(handle, eventMask);
    }

    void emulateSocketWriteEvent(SYSSOCKET handle, int eventMask)
    {
        m_udtEpollDummyWrapper->writeEvents.emplace(handle, eventMask);
    }

    std::size_t emulatedEventCount()
    {
        return m_udtEpollDummyWrapper->emulatedEventCount();
    }

    std::vector<std::unique_ptr<TCPSocket>> m_sockets;
    std::unique_ptr<aio::UnifiedPollSet> m_pollset;

private:
    UdtEpollDummyWrapper* m_udtEpollDummyWrapper = nullptr;
};

TEST_F(UnifiedPollSetSpecificTests, removing_socket_which_has_error_event_reported)
{
    const int kSocketCount = 2;

    initializeMultipleSockets(kSocketCount);

    emulateSocketReadEvent(m_sockets[0]->handle(), UDT_EPOLL_IN);
    emulateSocketReadEvent(m_sockets[1]->handle(), UDT_EPOLL_ERR);
    //emulateSocketWriteEvent(m_sockets[1]->handle(), UDT_EPOLL_ERR);

    ASSERT_EQ((int)emulatedEventCount(), m_pollset->poll());

    m_pollset->remove(m_sockets[1].get(), aio::etRead);

    for (auto it = m_pollset->begin(); it != m_pollset->end(); ++it)
    {
        ASSERT_EQ(m_sockets[0].get(), it.socket());
        ASSERT_EQ(aio::etRead, it.eventType());
    }
}

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
