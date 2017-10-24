#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <nx/utils/test_support/test_options.h>

#include <nx/cloud/cdb/test_support/cdb_launcher.h>

#include <test_support/peer_wrapper.h>
#include <test_support/merge_test_fixture.h>

namespace test {

class CloudMerge:
    public ::testing::Test
{
public:
    static void SetUpTestCase()
    {
        s_staticCommonModule = std::make_unique<QnStaticCommonModule>(
            Qn::PeerType::PT_Server);
    }

    static void TearDownTestCase()
    {
        s_staticCommonModule.reset();
    }

protected:
    void givenTwoCloudSystemsWithTheSameOwner()
    {
        ASSERT_TRUE(m_systemMergeFixture.initializeSingleServerSystems(2));
        // TODO Connecting systems to the cloud.
    }

    void whenMergeSystems()
    {
        m_systemMergeFixture.whenMergeSystems();
    }

    void thenSystemsMerged()
    {
        ASSERT_EQ(QnRestResult::Error::NoError, m_systemMergeFixture.prevMergeResult());
        m_systemMergeFixture.thenAllServersSynchronizedData();
    }

private:
    nx::cdb::CdbLauncher m_cdb;
    ::ec2::test::SystemMergeFixture m_systemMergeFixture;
    nx::cdb::AccountWithPassword m_ownerAccount;

    static std::unique_ptr<QnStaticCommonModule> s_staticCommonModule;

    virtual void SetUp() override
    {
        ASSERT_TRUE(m_cdb.startAndWaitUntilStarted());

        m_ownerAccount = m_cdb.addActivatedAccount2();

        //m_systemMergeFixture.addSetting(nx_ms_conf::CDB_ENDPOINT, m_cdb.endpoint().toString());
        //m_systemMergeFixture.addSetting(nx_ms_conf::DELAY_BEFORE_SETTING_MASTER_FLAG, "100ms");
    }
};

std::unique_ptr<QnStaticCommonModule> CloudMerge::s_staticCommonModule;

//TEST_F(CloudMerge, cloud_systems_with_the_same_owner_can_be_merged)
//{
//    givenTwoCloudSystemsWithTheSameOwner();
//    whenMergeSystems();
//    thenSystemsMerged();
//}

} // namespace test
