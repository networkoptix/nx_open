#include <memory>

#include <gtest/gtest.h>

#include <nx/network/cloud/multiple_address_connector.h>
#include <nx/network/socket_attributes_cache.h>
#include <nx/network/socket_global.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

namespace {

class UnresponsiveRemoteHostTcpSocketStub:
    public TCPSocket
{
public:
    UnresponsiveRemoteHostTcpSocketStub(int ipVersion):
        TCPSocket(ipVersion)
    {
    }

    virtual void connectAsync(
        const SocketAddress& /*addr*/,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> /*handler*/) override
    {
        // Just ignoring request to emulate unresponsive server.
    }
};

class FailingRemoteHostTcpSocketStub:
    public TCPSocket
{
public:
    FailingRemoteHostTcpSocketStub(int ipVersion):
        TCPSocket(ipVersion)
    {
    }

    virtual void connectAsync(
        const SocketAddress& /*addr*/,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override
    {
        post([handler = std::move(handler)]() { handler(SystemError::connectionRefused); });
    }
};

template<typename SocketType>
class ConnectorStub:
    public cloud::MultipleAddressConnector
{
    using base_type = cloud::MultipleAddressConnector;

public:
    ConnectorStub(
        int ipVersion,
        std::deque<AddressEntry> entries)
        :
        base_type(ipVersion, std::move(entries))
    {
    }

protected:
    virtual std::unique_ptr<AbstractStreamSocket> createTcpSocket(int ipVersion) override
    {
        return std::make_unique<SocketType>(ipVersion);
    }
};

using FailingHostConnector = ConnectorStub<FailingRemoteHostTcpSocketStub>;
using UnresponsiveHostConnector = ConnectorStub<UnresponsiveRemoteHostTcpSocketStub>;

// TODO: #ak in 3.1 remove following methods.

template<typename Value>
bool verifyAttribute(
    const AbstractSocket& socket,
    bool (AbstractSocket::*getAttributeFunc)(Value*) const,
    const boost::optional<Value>& expected)
{
    if (!expected)
        return true;

    Value actual{};
    if (!(socket.*getAttributeFunc)(&actual))
        return false;

    return *expected == actual;
}

bool verifySocketAttributes(
    const AbstractSocket& socket,
    const SocketAttributes& attributes)
{
    if (attributes.aioThread)
    {
        if (socket.getAioThread() != *attributes.aioThread)
            return false;
    }

    return verifyAttribute(socket, &AbstractSocket::getReuseAddrFlag, attributes.reuseAddrFlag)
        && verifyAttribute(socket, &AbstractSocket::getNonBlockingMode, attributes.nonBlockingMode)
        && verifyAttribute(socket, &AbstractSocket::getSendBufferSize, attributes.sendBufferSize)
        && verifyAttribute(socket, &AbstractSocket::getRecvBufferSize, attributes.recvBufferSize)
        && verifyAttribute(socket, &AbstractSocket::getRecvTimeout, attributes.recvTimeout)
        && verifyAttribute(socket, &AbstractSocket::getSendTimeout, attributes.sendTimeout);
}

} // namespace

//-------------------------------------------------------------------------------------------------

class MultipleAddressConnector:
    public ::testing::Test
{
public:
    MultipleAddressConnector()
    {
        m_socketAttributes.recvTimeout = 
            nx::utils::random::number<unsigned int>(1, 100000);
        m_socketAttributes.sendTimeout =
            nx::utils::random::number<unsigned int>(1, 100000);
        m_socketAttributes.aioThread = SocketGlobals::aioService().getRandomAioThread();
    }

    ~MultipleAddressConnector()
    {
        if (m_connector)
            m_connector->pleaseStopSync();

        for (auto& tcpServer: m_tcpServers)
            tcpServer->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        const int serverCount = 3;

        for (int i = 0; i < serverCount; ++i)
            addServer();
    }

    void setConnectTimeout(std::chrono::milliseconds timeout)
    {
        m_timeout = timeout;
    }

    void whenConnect()
    {
        initiateConnection(prepareEntries());
    }

    void whenConnectSpecifyingEmptyAddressList()
    {
        initiateConnection({});
    }

    void whenConnectToUnresponsiveHost()
    {
        initiateConnection<UnresponsiveHostConnector>(prepareEntries());
    }

    void whenConnectToFailingHost()
    {
        initiateConnection<FailingHostConnector>(prepareEntries());
    }

    void thenConnectionIsEstablished()
    {
        m_lastResult = m_connectResultQueue.pop();
        ASSERT_EQ(SystemError::noError, m_lastResult.sysErrorCode);
        ASSERT_NE(nullptr, m_lastResult.connection);
    }

    void thenConnectFailedWithResult(SystemError::ErrorCode sysErrorCode)
    {
        m_lastResult = m_connectResultQueue.pop();
        ASSERT_EQ(sysErrorCode, m_lastResult.sysErrorCode);
    }

    void andSocketAttributesHaveBeenAppliedToTheConnection()
    {
        ASSERT_TRUE(verifySocketAttributes(*m_lastResult.connection, m_socketAttributes));
    }

    void thenConnectFailed()
    {
        m_lastResult = m_connectResultQueue.pop();
        ASSERT_NE(SystemError::noError, m_lastResult.sysErrorCode);
    }

private:
    struct Result
    {
        SystemError::ErrorCode sysErrorCode = SystemError::noError;
        std::unique_ptr<AbstractStreamSocket> connection;

        Result() {}
        Result(
            SystemError::ErrorCode sysErrorCode,
            std::unique_ptr<AbstractStreamSocket> connection)
            :
            sysErrorCode(sysErrorCode),
            connection(std::move(connection))
        {
        }
    };

    std::vector<std::unique_ptr<network::test::RandomDataTcpServer>> m_tcpServers;
    std::unique_ptr<cloud::MultipleAddressConnector> m_connector;
    nx::utils::SyncQueue<Result> m_connectResultQueue;
    std::chrono::milliseconds m_timeout = std::chrono::milliseconds::zero();
    Result m_lastResult;
    StreamSocketAttributes m_socketAttributes;

    SocketAddress addServer()
    {
        auto tcpServer = std::make_unique<network::test::RandomDataTcpServer>(
            network::test::TestTrafficLimitType::none,
            0,
            network::test::TestTransmissionMode::spam);
        tcpServer->setLocalAddress(SocketAddress(HostAddress::localhost, 0));
        NX_GTEST_ASSERT_TRUE(tcpServer->start());
        const auto serverEndpoint = tcpServer->addressBeingListened();

        m_tcpServers.push_back(std::move(tcpServer));
        return serverEndpoint;
    }

    template<typename ConnectorType = cloud::MultipleAddressConnector>
    void initiateConnection(std::deque<AddressEntry> entries)
    {
        using namespace std::placeholders;

        m_connector = std::make_unique<ConnectorType>(
            AF_INET,
            std::move(entries));
        m_connector->connectAsync(
            m_timeout,
            m_socketAttributes,
            std::bind(&MultipleAddressConnector::onConnectCompletion, this, _1, _2));
    }

    std::deque<AddressEntry> prepareEntries()
    {
        std::deque<AddressEntry> entries;
        for (const auto& server: m_tcpServers)
            entries.push_back(AddressEntry(server->addressBeingListened()));
        return entries;
    }

    void onConnectCompletion(
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<AbstractStreamSocket> connection)
    {
        m_connectResultQueue.push(Result(sysErrorCode, std::move(connection)));
    }
};

//-------------------------------------------------------------------------------------------------

TEST_F(MultipleAddressConnector, connection_is_established_via_regular_entry)
{
    whenConnect();
    thenConnectionIsEstablished();
}

TEST_F(MultipleAddressConnector, empty_entry_list_produces_error)
{
    whenConnectSpecifyingEmptyAddressList();
    thenConnectFailedWithResult(SystemError::invalidData);
}

TEST_F(MultipleAddressConnector, timeout_is_reported)
{
    setConnectTimeout(std::chrono::milliseconds(1));

    whenConnectToUnresponsiveHost();
    thenConnectFailedWithResult(SystemError::timedOut);
}

TEST_F(MultipleAddressConnector, error_is_reported_if_every_entry_is_not_accessible)
{
    whenConnectToFailingHost();
    thenConnectFailed();
}

// TEST_F(MultipleAddressConnector, connection_is_established_via_cloud_entry)

TEST_F(MultipleAddressConnector, socket_attributes_are_applied_for_direct_connection)
{
    whenConnect();

    thenConnectionIsEstablished();
    andSocketAttributesHaveBeenAppliedToTheConnection();
}

// TEST_F(MultipleAddressConnector, socket_attributes_are_applied_for_cloud_connection)

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
