#include <gtest/gtest.h>

#include "ec2/cloud_vms_synchro_test_helper.h"
#include "ec2/transaction_connection_helper.h"

namespace nx {
namespace cdb {

constexpr auto timeForConnectionToChangeState = std::chrono::seconds(10);
constexpr auto allowedConnectionClosureTimeError = std::chrono::seconds(10);

class Ec2MserverCloudSynchronizationKeepAlive:
    public Ec2MserverCloudSynchronization
{
public:
    Ec2MserverCloudSynchronizationKeepAlive():
        m_connectionId(0)
    {
    }

protected:
    std::chrono::milliseconds m_connectionInactivityTimeout;
    TransactionConnectionHelper m_transactionConnectionHelper;
    int m_connectionId;

    void testTransactionConnectionKeepAlive(
        KeepAlivePolicy keepAlivePolicy);
};

TEST_F(Ec2MserverCloudSynchronizationKeepAlive, connection_break)
{
    testTransactionConnectionKeepAlive(KeepAlivePolicy::noKeepAlive);

    // Waiting for connection to be dropped by cloud due to absense of keep-alive messages.
    ASSERT_TRUE(
        m_transactionConnectionHelper.waitForState(
            {::ec2::QnTransactionTransportBase::Error, ::ec2::QnTransactionTransportBase::Closed},
            m_connectionId,
            m_connectionInactivityTimeout + allowedConnectionClosureTimeError));
}

TEST_F(Ec2MserverCloudSynchronizationKeepAlive, works)
{
    testTransactionConnectionKeepAlive(KeepAlivePolicy::enableKeepAlive);

    std::this_thread::sleep_for(m_connectionInactivityTimeout + allowedConnectionClosureTimeError);
    const auto connectionState =
        m_transactionConnectionHelper.getConnectionStateById(m_connectionId);
    ASSERT_TRUE(
        connectionState == ::ec2::QnTransactionTransportBase::Connected ||
        connectionState == ::ec2::QnTransactionTransportBase::ReadyForStreaming);
}

void Ec2MserverCloudSynchronizationKeepAlive::testTransactionConnectionKeepAlive(
    KeepAlivePolicy keepAlivePolicy)
{
    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
    ASSERT_EQ(api::ResultCode::ok, registerAccountAndBindSystemToIt());

    m_connectionId =
        m_transactionConnectionHelper.establishTransactionConnection(
            cdb()->endpoint(),
            registeredSystemData().id,
            registeredSystemData().authKey,
            keepAlivePolicy);

    m_transactionConnectionHelper.getAccessToConnectionById(
        m_connectionId,
        [this](test::TransactionTransport* const connection)
        {
            m_connectionInactivityTimeout =
                connection->connectionKeepAliveTimeout() *
                connection->keepAliveProbeCount();
        });

    ASSERT_TRUE(
        m_transactionConnectionHelper.waitForState(
            {::ec2::QnTransactionTransportBase::Connected,
                ::ec2::QnTransactionTransportBase::ReadyForStreaming},
            m_connectionId,
            timeForConnectionToChangeState));
}

} // namespace cdb
} // namespace nx
