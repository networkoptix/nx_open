#include <memory>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/outgoing_tunnel_pool.h>
#include <nx/network/cloud/tunnel/tcp/reverse_connection_holder.h>
#include <nx/network/cloud/tunnel/tcp/reverse_connection_pool.h>
#include <nx/network/cloud/tunnel/tcp/reverse_connector.h>
#include <nx/network/socket_global.h>
#include <nx/network/test_support/stun_async_client_mock.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {
namespace test {

namespace {

class MediatorClientTcpConnectionStub:
    public hpm::api::AbstractMediatorClientTcpConnection
{
    using base_type = hpm::api::AbstractMediatorClientTcpConnection;

public:
    template<typename ... Args>
    MediatorClientTcpConnectionStub(Args ... args):
        base_type(std::forward<Args>(args) ...)
    {
    }

    virtual void resolveDomain(
        nx::hpm::api::ResolveDomainRequest /*resolveData*/,
        utils::MoveOnlyFunc<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ResolveDomainResponse)> completionHandler) override
    {
        post(
            [completionHandler = std::move(completionHandler)]()
            {
                completionHandler(hpm::api::ResultCode::ok, hpm::api::ResolveDomainResponse());
            });
    }

    virtual void resolvePeer(
        nx::hpm::api::ResolvePeerRequest /*resolveData*/,
        utils::MoveOnlyFunc<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ResolvePeerResponse)> completionHandler) override
    {
        post(
            [completionHandler = std::move(completionHandler)]()
            {
                completionHandler(hpm::api::ResultCode::ok, hpm::api::ResolvePeerResponse());
            });
    }

    virtual void connect(
        nx::hpm::api::ConnectRequest /*connectData*/,
        utils::MoveOnlyFunc<void(
            stun::TransportHeader /*stunTransportHeader*/,
            nx::hpm::api::ResultCode,
            nx::hpm::api::ConnectResponse)> completionHandler) override
    {
        post(
            [completionHandler = std::move(completionHandler)]()
            {
                completionHandler(
                    stun::TransportHeader(),
                    hpm::api::ResultCode::ok,
                    hpm::api::ConnectResponse());
            });
    }

    virtual void bind(
        nx::hpm::api::ClientBindRequest /*request*/,
        utils::MoveOnlyFunc<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ClientBindResponse)> completionHandler) override
    {
        post(
            [completionHandler = std::move(completionHandler)]()
            {
                completionHandler(hpm::api::ResultCode::ok, hpm::api::ClientBindResponse());
            });
    }
};

} // namespace

//-------------------------------------------------------------------------------------------------

class ReverseConnectionPool:
    public ::testing::Test
{
public:
    ReverseConnectionPool():
        m_acceptingHostName(lm("%1.%1").args(QnUuid::createUuid().toSimpleByteArray()).toUtf8()),
        m_reverseConnectionPool(
            std::make_unique<MediatorClientTcpConnectionStub>(
                std::make_shared<stun::test::AsyncClientMock>())),
        m_reverseConnector(
            m_acceptingHostName,
            nx::network::SocketGlobals::outgoingTunnelPool().ownPeerId())
    {
        ReverseConnectionHolder::setConnectionlessHolderExpirationTimeout(
            std::chrono::seconds(31));
    }

    ~ReverseConnectionPool()
    {
        m_reverseConnectionPool.pleaseStopSync();
    }

protected:
    void givenOnlineHostWithReverseConnections()
    {
        whenEstablishReverseConnection();

        thenReverseConnectionIsEstablished();
        andConnectionSourceBecomesAvailable();
    }

    void whenEstablishReverseConnection()
    {
        using namespace std::placeholders;

        m_reverseConnector.connect(
            SocketAddress(HostAddress::localhost, m_reverseConnectionPool.port()),
            std::bind(&ReverseConnectionPool::saveReverseConnectorResult, this, _1));
    }

    void whenCloseReverseConnection()
    {
        m_reverseConnector.pleaseStopSync();
    }

    void andWaitUntilThereIsNoReverseConnectionsInPool()
    {
        for (;;)
        {
            auto connectionSource =
                m_reverseConnectionPool.getConnectionSource(m_acceptingHostName);
            if (!connectionSource || connectionSource->socketCount() == 0)
                return;

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void thenReverseConnectionIsEstablished()
    {
        ASSERT_EQ(SystemError::noError, m_reverseConnectorResults.pop());
    }

    void thenConnectionSourceIsAvailable()
    {
        m_reverseConnectionSource = 
            m_reverseConnectionPool.getConnectionSource(m_acceptingHostName);
        ASSERT_NE(nullptr, m_reverseConnectionSource);
    }

    void andConnectionSourceIsEmpty()
    {
        ASSERT_EQ(0U, m_reverseConnectionSource->socketCount());
    }

    void andConnectionSourceBecomesAvailable()
    {
        while (m_reverseConnectionPool.getConnectionSource(m_acceptingHostName) == nullptr)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    void thenConnectionSourceBecomesUnavailable()
    {
        while (m_reverseConnectionPool.getConnectionSource(m_acceptingHostName) != nullptr)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

private:
    nx::String m_acceptingHostName;
    tcp::ReverseConnectionPool m_reverseConnectionPool;
    tcp::ReverseConnector m_reverseConnector;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_reverseConnectorResults;
    std::shared_ptr<ReverseConnectionSource> m_reverseConnectionSource;

    virtual void SetUp() override
    {
        ASSERT_TRUE(m_reverseConnectionPool.start("1.2.3.4", 0, true));
    }

    void saveReverseConnectorResult(SystemError::ErrorCode systemErrorCode)
    {
        m_reverseConnectorResults.push(systemErrorCode);
    }
};

TEST_F(ReverseConnectionPool, not_expired_connection_source_with_no_connections_is_still_provided)
{
    givenOnlineHostWithReverseConnections();

    whenCloseReverseConnection();
    andWaitUntilThereIsNoReverseConnectionsInPool();

    thenConnectionSourceIsAvailable();
    andConnectionSourceIsEmpty();
}

TEST_F(ReverseConnectionPool, connection_source_without_connections_finally_expires)
{
    ReverseConnectionHolder::setConnectionlessHolderExpirationTimeout(
        std::chrono::milliseconds(1));

    givenOnlineHostWithReverseConnections();
    whenCloseReverseConnection();
    thenConnectionSourceBecomesUnavailable();
}

} // namespace test
} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
