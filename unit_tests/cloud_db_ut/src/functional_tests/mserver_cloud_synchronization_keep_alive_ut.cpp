#include <gtest/gtest.h>

#include <nx/network/url/url_builder.h>

#include <nx_ec/ec_proto_version.h>
#include <nx/cloud/cdb/test_support/transaction_connection_helper.h>

#include "ec2/cloud_vms_synchro_test_helper.h"

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
    test::TransactionConnectionHelper m_transactionConnectionHelper;
    int m_connectionId;

    void testTransactionConnectionKeepAlive(
        test::KeepAlivePolicy keepAlivePolicy);
};

TEST_P(Ec2MserverCloudSynchronizationKeepAlive, connection_break)
{
    testTransactionConnectionKeepAlive(test::KeepAlivePolicy::noKeepAlive);

    // Waiting for connection to be dropped by cloud due to absense of keep-alive messages.
    ASSERT_TRUE(
        m_transactionConnectionHelper.waitForState(
            {::ec2::QnTransactionTransportBase::Error, ::ec2::QnTransactionTransportBase::Closed},
            m_connectionId,
            m_connectionInactivityTimeout + allowedConnectionClosureTimeError));
}

TEST_P(Ec2MserverCloudSynchronizationKeepAlive, works)
{
    testTransactionConnectionKeepAlive(test::KeepAlivePolicy::enableKeepAlive);

    std::this_thread::sleep_for(m_connectionInactivityTimeout + allowedConnectionClosureTimeError);
    const auto connectionState =
        m_transactionConnectionHelper.getConnectionStateById(m_connectionId);
    ASSERT_TRUE(
        connectionState == ::ec2::QnTransactionTransportBase::Connected ||
        connectionState == ::ec2::QnTransactionTransportBase::ReadyForStreaming);
}

void Ec2MserverCloudSynchronizationKeepAlive::testTransactionConnectionKeepAlive(
    test::KeepAlivePolicy keepAlivePolicy)
{
    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
    ASSERT_EQ(api::ResultCode::ok, registerAccountAndBindSystemToIt());

    m_connectionId =
        m_transactionConnectionHelper.establishTransactionConnection(
            network::url::Builder().setScheme("http")
                .setHost(cdb()->endpoint().address.toString()).setPort(cdb()->endpoint().port),
            registeredSystemData().id,
            registeredSystemData().authKey,
            keepAlivePolicy,
            nx_ec::EC2_PROTO_VERSION);

    m_transactionConnectionHelper.getAccessToConnectionById(
        m_connectionId,
        [this](test::TransactionConnectionHelper::ConnectionContext* connectionContext)
        {
            m_connectionInactivityTimeout =
                connectionContext->connection->connectionKeepAliveTimeout() *
                connectionContext->connection->keepAliveProbeCount();
        });

    ASSERT_TRUE(
        m_transactionConnectionHelper.waitForState(
            {::ec2::QnTransactionTransportBase::Connected,
                ::ec2::QnTransactionTransportBase::ReadyForStreaming},
            m_connectionId,
            timeForConnectionToChangeState));
}

INSTANTIATE_TEST_CASE_P(P2pMode, Ec2MserverCloudSynchronizationKeepAlive,
    ::testing::Values(TestParams(false), TestParams(true)
));

} // namespace cdb
} // namespace nx
