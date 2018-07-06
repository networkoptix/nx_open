#include <gtest/gtest.h>

#include <nx/utils/thread/sync_queue.h>

#include <nx/data_sync_engine/compatible_ec2_protocol_version.h>

#include "mserver_cloud_synchronization_connection_fixture.h"

namespace nx {
namespace cdb {
namespace test {

class Ec2MserverCloudCompatibility:
    public Ec2MserverCloudSynchronizationConnection
{
public:
    Ec2MserverCloudCompatibility()
    {
        onConnectionBecomesActiveSubscription().subscribe(
            [this](int connectionId, ::ec2::QnTransactionTransportBase::State /*state*/)
            {
                m_connectionStateChangeQueue.push({ connectionId, true });
            },
            &m_onConnectionBecomesActiveSubscriptionId);

        onConnectionFailureSubscription().subscribe(
            [this](int connectionId, ::ec2::QnTransactionTransportBase::State /*state*/)
            {
                m_connectionStateChangeQueue.push({ connectionId, false });
            },
            &m_onConnectionFailureSubscriptionId);
    }

    ~Ec2MserverCloudCompatibility()
    {
        onConnectionBecomesActiveSubscription().removeSubscription(
            m_onConnectionBecomesActiveSubscriptionId);

        onConnectionFailureSubscription().removeSubscription(
            m_onConnectionFailureSubscriptionId);
    }

protected:
    void assertCdbAcceptsConnectionOfVersion(int version)
    {
        openConnectionAndWaitForItToMoveToState(version, true);
        waitUntilActiveConnectionCountReaches(1);
    }

    void assertCdbDoesNotAcceptConnectionOfVersion(int version)
    {
        openConnectionAndWaitForItToMoveToState(version, false);
        waitUntilClosedConnectionCountReaches(1);
    }

private:
    struct ConnectionStateChangeContext
    {
        int connectionId;
        bool isActive;
    };

    nx::utils::SyncQueue<ConnectionStateChangeContext> m_connectionStateChangeQueue;
    nx::utils::SubscriptionId m_onConnectionBecomesActiveSubscriptionId = -1;
    nx::utils::SubscriptionId m_onConnectionFailureSubscriptionId = -1;

    void openConnectionAndWaitForItToMoveToState(int version, bool isActive)
    {
        std::vector<int> createdConnectionIds =
            openTransactionConnectionsOfSpecifiedVersion(1, version);
        const auto connectionId = createdConnectionIds[0];
        for (;;)
        {
            const auto connectionStateChangeContext = m_connectionStateChangeQueue.pop();
            if (connectionStateChangeContext.connectionId != connectionId)
                continue;
            ASSERT_EQ(isActive, connectionStateChangeContext.isActive);
            break;
        }
    }
};

TEST_F(Ec2MserverCloudCompatibility, compatible_protocol_range_is_meaningful)
{
    ASSERT_LE(
        data_sync_engine::kMinSupportedProtocolVersion,
        data_sync_engine::kMaxSupportedProtocolVersion);
}

TEST_F(Ec2MserverCloudCompatibility, any_compatible_proto_version_is_accepted_by_cloud)
{
    for (int
        version = data_sync_engine::kMinSupportedProtocolVersion;
        version <= data_sync_engine::kMaxSupportedProtocolVersion;
        ++version)
    {
        assertCdbAcceptsConnectionOfVersion(version);
        useAnotherSystem();
    }
}

TEST_F(Ec2MserverCloudCompatibility, version_left_of_compatibility_range_is_rejected)
{
    assertCdbDoesNotAcceptConnectionOfVersion(
        data_sync_engine::kMinSupportedProtocolVersion - 1);
}

TEST_F(Ec2MserverCloudCompatibility, version_right_of_compatibility_range_is_rejected)
{
    assertCdbDoesNotAcceptConnectionOfVersion(
        data_sync_engine::kMaxSupportedProtocolVersion + 1);
}

//-------------------------------------------------------------------------------------------------

TEST(Ec2MserverCloudCompabilityCheckRoutine, compatible_versions)
{
    for (int
        version = data_sync_engine::kMinSupportedProtocolVersion;
        version <= data_sync_engine::kMaxSupportedProtocolVersion;
        ++version)
    {
        ASSERT_TRUE(data_sync_engine::isProtocolVersionCompatible(version));
    }
}

TEST(Ec2MserverCloudCompabilityCheckRoutine, incompatible_versions)
{
    ASSERT_FALSE(data_sync_engine::isProtocolVersionCompatible(
        data_sync_engine::kMinSupportedProtocolVersion - 1));
    ASSERT_FALSE(data_sync_engine::isProtocolVersionCompatible(
        data_sync_engine::kMaxSupportedProtocolVersion + 1));
}

} // namespace test
} // namespace cdb
} // namespace nx
