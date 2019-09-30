#include <gtest/gtest.h>

#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/cloud/storage/service/test_support/cloud_storage_launcher.h>

#include <nx/cloud/storage/service/settings.h>
#include <nx/cloud/storage/service/controller/geo_ip_resolver.h>
#include <nx/geo_ip/test_support/memory_resolver.h>

namespace nx::cloud::storage::service::test {

class CloudStorageService:
    public testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
public:
    ~CloudStorageService()
    {
        if (m_geoIpFactoryFuncBak)
        {
            controller::GeoIpResolverFactory::instance().setCustomFunc(
                std::move(m_geoIpFactoryFuncBak));
        }
    }
protected:
    virtual void SetUp() override
    {
        m_geoIpFactoryFuncBak =
            controller::GeoIpResolverFactory::instance().setCustomFunc(
                [](auto&& ... /*args*/)
                {
                    return std::make_unique<nx::geo_ip::test::MemoryResolver>();
                });

        m_cloudStorage = std::make_unique<CloudStorageLauncher>();
    }

    void givenSettings()
    {
        m_cloudStorage->addArg("-http/tcpBacklogSize", "130");
        m_cloudStorage->addArg("-http/connectionInactivityPeriod", "1h");
        m_cloudStorage->addArg("-http/sslEndpoints", "127.0.0.1:0");
        m_htdigestPath = (testDataDir() + "/htdigest.txt").toStdString();
        m_cloudStorage->addArg("-http/htdigestPath", m_htdigestPath.c_str());

        m_cloudStorage->addArg("-statistics/enabled", "true");

        m_cloudStorage->addArg("-server/name", "test_settings_server");
    }

    void whenStartCloudStorageService()
    {
        ASSERT_TRUE(m_cloudStorage->startAndWaitUntilStarted());
    }

    void thenSettingsAreLoaded()
    {
        const conf::Settings& settings = m_cloudStorage->moduleInstance()->settings();

        ASSERT_EQ(130, settings.http().tcpBacklogSize);
        ASSERT_EQ(1, (int) settings.http().endpoints.size());
        ASSERT_EQ(1, (int) settings.http().sslEndpoints.size());
        ASSERT_EQ(std::chrono::hours(1), settings.http().connectionInactivityPeriod);
        ASSERT_EQ(m_htdigestPath, settings.http().htdigestPath);

        ASSERT_EQ(true, settings.statistics().enabled);

        ASSERT_EQ("test_settings_server", settings.server().name);
    }

private:
    std::unique_ptr<CloudStorageLauncher> m_cloudStorage;
    std::string m_htdigestPath;
    controller::GeoIpResolverFactory::Function m_geoIpFactoryFuncBak;
};

TEST_F(CloudStorageService, loads_all_settings)
{
    givenSettings();

    whenStartCloudStorageService();

    thenSettingsAreLoaded();
}

} // namespace nx::cloud::storage::service::test