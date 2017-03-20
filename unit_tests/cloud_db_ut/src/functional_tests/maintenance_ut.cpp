#include <gtest/gtest.h>

#include "mserver_cloud_synchronization_connection_fixture.h"

namespace nx {
namespace cdb {
namespace test {

class FtMaintenance:
    public Ec2MserverCloudSynchronizationConnection
{
protected:
    void assertOnlineServerCountIsEqualTo(int count)
    {
        api::Statistics statistics;
        ASSERT_EQ(api::ResultCode::ok, getStatistics(&statistics));
        ASSERT_EQ(count, statistics.onlineServerCount);
    }
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

} // namespace test
} // namespace cdb
} // namespace nx
