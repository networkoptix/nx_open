#include <gtest/gtest.h>

#include "mserver_cloud_synchronization_connection_fixture.h"

namespace nx {
namespace cdb {
namespace test {

class FtMaintenance:
    public Ec2MserverCloudSynchronizationConnection
{
protected:
    void whenRequestTransactionLog()
    {
        m_prevRequestResult = getTransactionLog(
            account().email,
            account().password,
            system().id,
            &m_transactionLog);
    }

    void thenTransactionLogIsProvided()
    {
        ASSERT_EQ(api::ResultCode::ok, m_prevRequestResult);
    }

    void assertOnlineServerCountIsEqualTo(int count)
    {
        api::Statistics statistics;
        ASSERT_EQ(api::ResultCode::ok, getStatistics(&statistics));
        ASSERT_EQ(count, statistics.onlineServerCount);
    }

private:
    ::ec2::ApiTransactionDataList m_transactionLog;
    api::ResultCode m_prevRequestResult = api::ResultCode::ok;
};

TEST_F(FtMaintenance, statistics_online_server_count)
{
    constexpr int connectionCount = 1;

    openTransactionConnections(connectionCount);
    waitUntilActiveConnectionCountReaches(connectionCount);
    assertOnlineServerCountIsEqualTo(connectionCount);
}

TEST_F(FtMaintenance, statistics_online_server_count_no_servers)
{
    assertOnlineServerCountIsEqualTo(0);
}

TEST_F(FtMaintenance, transaction_log_is_available_to_the_system_owner)
{
    whenRequestTransactionLog();
    thenTransactionLogIsProvided();
}

} // namespace test
} // namespace cdb
} // namespace nx
