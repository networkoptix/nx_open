#include <gtest/gtest.h>

#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <test_support/merge_test_fixture.h>

#include "cloud_system_fixture.h"

namespace test {

class DetachSystem:
    public ::testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
    using base_type = ::testing::Test;

public:
    DetachSystem():
        nx::utils::test::TestWithTemporaryDirectory(
            "vms_cloud_intergration.DetachSystem",
            QString()),
        m_cloudSystemFixture(testDataDir().toStdString())
    {
    }

    static void SetUpTestCase()
    {
        s_staticCommonModule =
            std::make_unique<QnStaticCommonModule>(nx::vms::api::PeerType::server);
    }

    static void TearDownTestCase()
    {
        s_staticCommonModule.reset();
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        ASSERT_TRUE(m_cloudSystemFixture.cloud().initialize());
    }

    void givenMultipleServerCloudSystem()
    {
        m_system = m_cloudSystemFixture.createVmsSystem(2);
        ASSERT_NE(nullptr, m_system);

        m_cloudAccount = m_cloudSystemFixture.cloud().registerCloudAccount();
        ASSERT_TRUE(m_system->connectToCloud(
            &m_cloudSystemFixture.cloud(), m_cloudAccount));
    }

    void whenDetachSingleServerFromSystem()
    {
        m_detachedSystem = m_system->detachServer(1);
    }

    void thenDetachSucceeded()
    {
        ASSERT_NE(nullptr, m_detachedSystem);
    }

    void andDetachedServerIsNotConnectedToCloud()
    {
        ASSERT_FALSE(m_detachedSystem->isConnectedToCloud());
    }

    void andCloudSystemIsStillThere()
    {
        ASSERT_TRUE(m_system->isConnectedToCloud());
        ASSERT_TRUE(m_cloudSystemFixture.cloud().isSystemRegistered(
            m_cloudAccount,
            m_system->cloudSystemId()));

        waitForSystemToBecomeOnline();
    }

    void waitForSystemToBecomeOnline()
    {
        for (;;)
        {
            if (m_cloudSystemFixture.cloud().isSystemOnline(m_cloudAccount, m_system->cloudSystemId()))
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

private:
    std::unique_ptr<VmsSystem> m_system;
    std::unique_ptr<VmsSystem> m_detachedSystem;
    nx::cloud::db::AccountWithPassword m_cloudAccount;
    CloudSystemFixture m_cloudSystemFixture;

    static std::unique_ptr<QnStaticCommonModule> s_staticCommonModule;
};

std::unique_ptr<QnStaticCommonModule> DetachSystem::s_staticCommonModule;

TEST_F(DetachSystem, server_is_detached_correctly_from_cloud_system)
{
    givenMultipleServerCloudSystem();

    whenDetachSingleServerFromSystem();

    thenDetachSucceeded();
    andDetachedServerIsNotConnectedToCloud();
    andCloudSystemIsStillThere();
}

} // namespace test
