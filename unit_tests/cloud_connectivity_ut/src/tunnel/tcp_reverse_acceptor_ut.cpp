#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/tcp/reverse_acceptor.h>
#include <nx/network/cloud/tunnel/tcp/reverse_connector.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {
namespace test {

static const String kAcceptorHostName("theAcceptor");

class TcpReverseAcceptor:
    public ::testing::Test
{
protected:
    TcpReverseAcceptor():
        m_acceptor(m_acceptResults.pusher())
    {
    }

    void testConnect(
        const String& hostName,
        boost::optional<size_t> poolSize,
        boost::optional<KeepAliveOptions> kaOptions,
        String testMessage,
        std::chrono::milliseconds waitBeforeUse)
    {
        NX_LOGX(lm("Test hostName=%1, poolSize=%2, keepAlive=%3, messageSize=%4, wait=%5")
            .args(hostName, poolSize, kaOptions, testMessage.size(), waitBeforeUse), cl_logDEBUG1);

        m_acceptor.setPoolSize(poolSize);
        m_acceptor.setKeepAliveOptions(kaOptions);

        auto connector = std::make_unique<ReverseConnector>(hostName, kAcceptorHostName);
        utils::promise<void> connectorDone;
        connector->connect(
            m_acceptorAddress,
            [&](SystemError::ErrorCode code)
            {
                ASSERT_EQ(code, SystemError::noError);
                ASSERT_EQ(connector->getPoolSize(), poolSize);
                ASSERT_EQ(connector->getKeepAliveOptions(), kaOptions);

                auto buffer = std::make_shared<Buffer>();
                buffer->reserve(1024);

                std::shared_ptr<nx::network::AbstractStreamSocket> socket(connector->takeSocket().release());
                auto socketRaw = socket.get();
                socketRaw->readAsyncAtLeast(
                    buffer.get(), buffer->capacity(),
                    [buffer, &connectorDone, &testMessage, socket = std::move(socket)](
                        SystemError::ErrorCode code, size_t) mutable
                    {
                        ASSERT_EQ(code, SystemError::noError);
                        ASSERT_EQ(*buffer, testMessage);

                        socket.reset();
                        connectorDone.set_value();
                    });
            });

        auto accepted = m_acceptResults.pop();
        ASSERT_EQ(accepted.first, hostName);
        ASSERT_TRUE((bool)accepted.second);
        ASSERT_TRUE(accepted.second->setNonBlockingMode(false));

        if (waitBeforeUse.count())
            std::this_thread::sleep_for(waitBeforeUse);

        if (!testMessage.isEmpty())
        {
            ASSERT_EQ(
                accepted.second->send(testMessage.data(), testMessage.size()),
                testMessage.size());
        }

        accepted.second.reset();
        connectorDone.get_future().wait();
    }

    void whenEstablishReverseConnection()
    {
        m_originatingHostName = nx::utils::generateRandomName(7);
        auto connector = std::make_unique<ReverseConnector>(
            m_originatingHostName,
            kAcceptorHostName);
        auto connectorGuard = makeScopeGuard([&connector]() { connector->pleaseStopSync(); });

        utils::promise<SystemError::ErrorCode> connectorDone;
        connector->connect(
            m_acceptorAddress,
            [&connectorDone](SystemError::ErrorCode resultCode)
            {
                connectorDone.set_value(resultCode);
            });
        connectorDone.get_future().wait();
    }

    void thenAcceptorProducesConnection()
    {
        m_prevAcceptedConnection = m_acceptResults.pop().second;
        ASSERT_NE(nullptr, m_prevAcceptedConnection.get());
    }

    void andAcceptedConnectionReportsProperForeignHostName()
    {
        ASSERT_EQ(m_originatingHostName, m_prevAcceptedConnection->getForeignHostName().toUtf8());
    }

private:
    utils::SyncMultiQueue<String, std::unique_ptr<nx::network::AbstractStreamSocket>> m_acceptResults;
    ReverseAcceptor m_acceptor;
    nx::network::SocketAddress m_acceptorAddress;
    nx::String m_originatingHostName;
    std::unique_ptr<nx::network::AbstractStreamSocket> m_prevAcceptedConnection;

    virtual void SetUp() override
    {
        ASSERT_TRUE(m_acceptor.start(kAcceptorHostName, nx::network::SocketAddress::anyAddress));
        m_acceptorAddress = nx::network::SocketAddress(nx::network::HostAddress::localhost, m_acceptor.address().port);
    }
};

TEST_F(TcpReverseAcceptor, Connect)
{
    for (const String& hostName: {"client1", "client2"})
    {
        typedef boost::optional<size_t> OptSize;
        for (const OptSize& poolSize: {OptSize(), OptSize(3), OptSize(5)})
        {
            typedef boost::optional<KeepAliveOptions> OptKa;
            const auto keepAliveOptionsArray = {
                OptKa(),
                OptKa(KeepAliveOptions(std::chrono::seconds(3), std::chrono::seconds(2), 1))};
            for (const OptKa& keepAlive: keepAliveOptionsArray)
            {
                for (const auto& message: {Buffer(""), nx::utils::random::generate(1024)})
                {
                    typedef std::chrono::milliseconds ms;
                    for (const ms& wait: {ms(0), ms(100), ms(500)})
                        testConnect( hostName, poolSize, keepAlive, message, wait);
                }
            }
        }
    }
}

TEST_F(TcpReverseAcceptor, resulting_socket_reports_proper_foreign_host_name)
{
    whenEstablishReverseConnection();

    thenAcceptorProducesConnection();
    andAcceptedConnectionReportsProperForeignHostName();
}

} // namespace test
} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
