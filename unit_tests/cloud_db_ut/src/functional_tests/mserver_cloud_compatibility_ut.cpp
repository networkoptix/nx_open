#include <gtest/gtest.h>

#include <ec2/compatible_ec2_protocol_version.h>

#include "mserver_cloud_synchronization_connection_fixture.h"

namespace nx {
namespace cdb {
namespace test {

class Ec2MserverCloudCompability:
    public Ec2MserverCloudSynchronizationConnection
{
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
    void openConnectionAndWaitForItToMoveToState(int version, bool isActive)
    {
        nx::utils::promise<bool> connectionStateChanged;
        setOnConnectionBecomesActive(
            [&connectionStateChanged](
                ::ec2::QnTransactionTransportBase::State /*state*/)
            {
                connectionStateChanged.set_value(true);
            });
        setOnConnectionFailure(
            [&connectionStateChanged](
                ::ec2::QnTransactionTransportBase::State /*state*/)
            {
                connectionStateChanged.set_value(false);
            });

        openTransactionConnectionsOfSpecifiedVersion(1, version);
        ASSERT_EQ(isActive, connectionStateChanged.get_future().get());
    }
};

TEST_F(Ec2MserverCloudCompability, compatible_protocol_range_is_meaningful)
{
    ASSERT_LE(ec2::kMinSupportedProtocolVersion, ec2::kMaxSupportedProtocolVersion);
}

TEST_F(Ec2MserverCloudCompability, any_compatible_proto_version_is_accepted_by_cloud)
{
    for (int
        version = ec2::kMinSupportedProtocolVersion;
        version <= ec2::kMaxSupportedProtocolVersion;
        ++version)
    {
        assertCdbAcceptsConnectionOfVersion(version);
    }
}

TEST_F(Ec2MserverCloudCompability, version_left_of_compatibility_range_is_rejected)
{
    assertCdbDoesNotAcceptConnectionOfVersion(ec2::kMinSupportedProtocolVersion - 1);
}

TEST_F(Ec2MserverCloudCompability, version_right_of_compatibility_range_is_rejected)
{
    assertCdbDoesNotAcceptConnectionOfVersion(ec2::kMaxSupportedProtocolVersion + 1);
}

//-------------------------------------------------------------------------------------------------

TEST(Ec2MserverCloudCompabilityCheckRoutine, compatible_versions)
{
    for (int
        version = ec2::kMinSupportedProtocolVersion;
        version <= ec2::kMaxSupportedProtocolVersion;
        ++version)
    {
        ASSERT_TRUE(ec2::isProtocolVersionCompatible(version));
    }
}

TEST(Ec2MserverCloudCompabilityCheckRoutine, incompatible_versions)
{
    ASSERT_FALSE(ec2::isProtocolVersionCompatible(ec2::kMinSupportedProtocolVersion - 1));
    ASSERT_FALSE(ec2::isProtocolVersionCompatible(ec2::kMaxSupportedProtocolVersion + 1));
}

} // namespace test 
} // namespace cdb
} // namespace nx
