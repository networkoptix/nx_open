#include <gtest/gtest.h>

#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/random.h>

#include <nx/cloud/db/client/cdb_request_path.h>

#include "mserver_cloud_synchronization_connection_fixture.h"

namespace nx::cloud::db::test {

static constexpr auto kWaitTimeout = std::chrono::minutes(1);

class Ec2SyncConnection:
    public Ec2MserverCloudSynchronizationConnection
{
protected:
    void whenConnect()
    {
        openTransactionConnections(1);
    }

    void thenConnectionisEstablished()
    {
        waitForConnectionsToMoveToACertainState(
            {::ec2::QnTransactionTransportBase::Connected,
                ::ec2::QnTransactionTransportBase::ReadyForStreaming},
            kWaitTimeout);
    }
};

TEST_F(Ec2SyncConnection, connection_drop_after_system_removal)
{
    static constexpr auto kConnectionsToCreateCount = 1;

    openTransactionConnections(kConnectionsToCreateCount);

    waitForConnectionsToMoveToACertainState(
        {::ec2::QnTransactionTransportBase::Connected,
            ::ec2::QnTransactionTransportBase::ReadyForStreaming},
        kWaitTimeout);

    ASSERT_EQ(
        api::ResultCode::ok,
        unbindSystem(account().email, account().password, system().id));

    waitForConnectionsToMoveToACertainState(
        {::ec2::QnTransactionTransportBase::Closed,
            ::ec2::QnTransactionTransportBase::Error},
        kWaitTimeout);
}

TEST_F(Ec2SyncConnection, multiple_connections)
{
    constexpr int maxAllowedConcurrentConnections = 2;
    constexpr int maxConcurrentConnectionsToCreate = 5;
    
    static_assert(
        maxConcurrentConnectionsToCreate > maxAllowedConcurrentConnections,
        "Check values");

    openTransactionConnections(maxConcurrentConnectionsToCreate);

    // Waiting for active connection count to reach maxAllowedConcurrentConnections
    waitUntilActiveConnectionCountReaches(maxAllowedConcurrentConnections);

    // Waiting until every other connection has been rejected.
    waitUntilClosedConnectionCountReaches(
        maxConcurrentConnectionsToCreate - maxAllowedConcurrentConnections);

    // Checking active connection count once again.
    ASSERT_EQ(maxAllowedConcurrentConnections, activeConnectionCount());
}

TEST_F(Ec2SyncConnection, cloud_db_is_stable_when_connections_blink)
{
    constexpr int maxConcurrentConnectionsToCreate = 50;
    constexpr int iterationCount = 2;

    for (int i = 0; i < iterationCount; ++i)
    {
        openTransactionConnections(maxConcurrentConnectionsToCreate);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(nx::utils::random::number<int>(100, 999)));
        closeAllConnections();
    }
}

//-------------------------------------------------------------------------------------------------

template<typename PathHolder>
class Ec2SyncConnectionPathAcceptance:
    public Ec2SyncConnection
{
public:
    Ec2SyncConnectionPathAcceptance()
    {
        this->connectionHelper().setSyncPath(
            nx::network::url::joinPath(PathHolder::kPath, "events"));
    }
};

TYPED_TEST_CASE_P(Ec2SyncConnectionPathAcceptance);

TYPED_TEST_P(Ec2SyncConnectionPathAcceptance, connection_can_be_established)
{
    this->whenConnect();
    this->thenConnectionisEstablished();
}

REGISTER_TYPED_TEST_CASE_P(Ec2SyncConnectionPathAcceptance,
    connection_can_be_established);

//-------------------------------------------------------------------------------------------------

struct RegularPathHolder { static constexpr auto kPath = kEc2TransactionConnectionPathPrefix; };
struct DeprecatedPathHolder { static constexpr auto kPath = kDeprecatedEc2TransactionConnectionPathPrefix; };

INSTANTIATE_TYPED_TEST_CASE_P(
    RegularPath,
    Ec2SyncConnectionPathAcceptance,
    RegularPathHolder);

INSTANTIATE_TYPED_TEST_CASE_P(
    DeprecatedPath,
    Ec2SyncConnectionPathAcceptance,
    DeprecatedPathHolder);

} // namespace nx::cloud::db::test
