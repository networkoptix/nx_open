#include <gtest/gtest.h>

#include <boost/optional.hpp>

#include <nx/network/connection_server/base_server_connection.h>
#include <nx/network/system_socket.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace server {
namespace test {

namespace {

class TestStreamSocket:
    public nx::network::TCPSocket
{
public:
    virtual bool setNonBlockingMode(bool /*val*/) override
    {
        SystemError::setLastErrorCode(SystemError::notConnected);
        return false;
    }
};

class TestConnection:
    public nx::network::server::BaseServerConnection<TestConnection>
{
    using base_type = nx::network::server::BaseServerConnection<TestConnection>;

public:
    TestConnection(
        StreamConnectionHolder<TestConnection>* connectionManager,
        std::unique_ptr<AbstractStreamSocket> streamSocket)
        :
        base_type(connectionManager, std::move(streamSocket))
    {
    }
        
    void bytesReceived(nx::Buffer& /*buffer*/)
    {
    }

    void readyToSendData()
    {
    }
};

} // namespace

//-------------------------------------------------------------------------------------------------

class ConnectionServerBaseServerConnection:
    public ::testing::Test,
    public nx::network::server::StreamConnectionHolder<TestConnection>
{
protected:
    void givenConnectionWithBrokenSocket()
    {
        m_connection = std::make_unique<TestConnection>(
            this,
            std::make_unique<TestStreamSocket>());
    }
    
    void whenInvokeStartReading()
    {
        nx::utils::promise<void> done;
        m_connection->post(
            [this, &done]()
            {
                m_invokingConnectionMethod = true;
                m_connection->startReadingConnection();
                m_invokingConnectionMethod = false;

                done.set_value();
            });
        done.get_future().wait();
    }

    void thenFailureIsReportedDelayed()
    {
        m_connectionCloseEvents.pop();
        ASSERT_FALSE(*m_connectionClosedReportedDirectly);
    }

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        ConnectionType* connection) override
    {
        ASSERT_EQ(m_connection.get(), connection);
        m_connection.reset();
        m_connectionClosedReportedDirectly = m_invokingConnectionMethod;

        m_connectionCloseEvents.push(closeReason);
    }

private:
    std::unique_ptr<TestConnection> m_connection;
    bool m_invokingConnectionMethod = false;
    boost::optional<bool> m_connectionClosedReportedDirectly;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_connectionCloseEvents;
};

TEST_F(ConnectionServerBaseServerConnection, start_read_failure_is_report_via_event_loop)
{
    givenConnectionWithBrokenSocket();
    whenInvokeStartReading();
    thenFailureIsReportedDelayed();
}

} // namespace test
} // namespace server
} // namespace network
} // namespace nx
