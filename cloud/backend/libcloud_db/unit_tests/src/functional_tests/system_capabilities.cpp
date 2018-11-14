#include <gtest/gtest.h>

#include <nx/utils/std/algorithm.h>
#include <nx/utils/test_support/utils.h>

#include <nx_ec/ec_proto_version.h>

#include "mserver_cloud_synchronization_connection_fixture.h"

namespace nx::cloud::db {
namespace test {

class SystemCapabilities:
    public Ec2MserverCloudSynchronizationConnection
{
protected:
    void setProtocolVersion(int protocolVersion)
    {
        m_ec2ProtocolVersion = protocolVersion;
    }

    void givenOnlineSystem()
    {
        openTransactionConnectionsOfSpecifiedVersion(1, m_ec2ProtocolVersion);
        waitForConnectionsToMoveToACertainState(
            {::ec2::QnTransactionTransportBase::Connected,
                ::ec2::QnTransactionTransportBase::ReadyForStreaming },
            std::chrono::hours(1));
    }

    void assertCloudMergeCapabilityIsSet()
    {
        ASSERT_TRUE(nx::utils::contains(
            fetchSystemCapabilities(),
            api::SystemCapabilityFlag::cloudMerge));
    }

    void assertCloudMergeCapabilityIsNotSet()
    {
        ASSERT_FALSE(nx::utils::contains(
            fetchSystemCapabilities(),
            api::SystemCapabilityFlag::cloudMerge));
    }

private:
    int m_ec2ProtocolVersion = nx_ec::EC2_PROTO_VERSION;

    std::vector<api::SystemCapabilityFlag> fetchSystemCapabilities()
    {
        api::SystemDataEx systemData;
        NX_GTEST_ASSERT_EQ(
            api::ResultCode::ok,
            getSystem(account().email, account().password, system().id, &systemData));
        return systemData.capabilities;
    }
};

TEST_F(SystemCapabilities, cloud_merge_is_reported_when_appropriate)
{
    givenOnlineSystem();
    assertCloudMergeCapabilityIsSet();
}

TEST_F(SystemCapabilities, cloud_merge_is_not_reported_for_old_protocol_version)
{
    constexpr int kProtocolVersionWithoutCloudMerge = 3040;
    setProtocolVersion(kProtocolVersionWithoutCloudMerge);

    givenOnlineSystem();
    assertCloudMergeCapabilityIsNotSet();
}

} // namespace test
} // namespace nx::cloud::db
