#include <gtest/gtest.h>

#include <nx/utils/test_support/utils.h>

#include "mediator_cloud_db_integration_test_setup.h"

namespace nx {
namespace cloud {
namespace test {

class MediatorCloudDbIntegration:
    public MediatorCloudDbIntegrationTestSetup,
    public ::testing::Test
{
public:
    MediatorCloudDbIntegration()
    {
        init();
    }

protected:
    void registerSystemsOfDifferentCustomizations()
    {
        const char* customizations[] = {"nx", "dw"};

        for (const auto customization: customizations)
        {
            nx::cloud::db::api::SystemData system;
            system.customization = customization;
            NX_GTEST_ASSERT_NO_THROW(
                system = cloudDb().addRandomSystemToAccount(m_account, system));
            m_systems.push_back(std::move(system));
        }
    }

    void assertMediatorAcceptsEverySystemCredentials()
    {
        ASSERT_TRUE(startMediator());

        bool allCloudSystemCredentialsHaveBeenAccepted = false;
        while (!allCloudSystemCredentialsHaveBeenAccepted)
        {
            allCloudSystemCredentialsHaveBeenAccepted = true;
            for (const auto& system: m_systems)
            {
                hpm::AbstractCloudDataProvider::System cloudSystemCredentials;
                cloudSystemCredentials.id = system.id.c_str();
                cloudSystemCredentials.authKey = system.authKey.c_str();
                cloudSystemCredentials.mediatorEnabled = true;
                auto mediaServer = mediator().addServer(cloudSystemCredentials, system.name.c_str());
                if (!mediaServer)
                {
                    allCloudSystemCredentialsHaveBeenAccepted = false;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    break;
                }
            }
        }
    }

private:
    nx::cloud::db::AccountWithPassword m_account;
    std::vector<nx::cloud::db::api::SystemData> m_systems;

    void init()
    {
        ASSERT_TRUE(startCdb());
        NX_GTEST_ASSERT_NO_THROW(m_account = cloudDb().addActivatedAccount2());
    }
};

TEST_F(MediatorCloudDbIntegration, mediator_serves_mediaserver_of_any_customization)
{
    registerSystemsOfDifferentCustomizations();
    assertMediatorAcceptsEverySystemCredentials();
}

} // namespace test
} // namespace cloud
} // namespace nx
